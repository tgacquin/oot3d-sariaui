// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
namespace F=OOT3D::FileSelect;
namespace K=OOT3D::NameKeyboard;
using U32=std::uint32_t;
struct Memory: Dynarmic::A32::UserCallbacks {
    static constexpr U32 Stop=0xf00000,SP=0xe00000;
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x1000000);
    Dynarmic::A32::Jit* cpu=nullptr;std::uint64_t ticks=0;
    F::Hook hook;U32 directions=0,pressed=0;unsigned move_sounds=0,validations=0;
    bool valid_name=true,save_done=false,save_testing=false;
    bool room_testing=false,typing_testing=false;
    std::int64_t now_us=0;
    unsigned save_queues=0,save_joins=0,name_teardowns=0;
    std::vector<U32> sounds;
    std::vector<std::array<float,12>> cursor_quads;
    std::vector<std::array<float,2>> cursor_origins;
    template<class T> T Get(U32 at) {assert(at+sizeof(T)<=bytes.size());T v;std::memcpy(&v,bytes.data()+at,sizeof(T));return v;}
    template<class T> void Put(U32 at,T v) {assert(at+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+at,&v,sizeof(T));}
    std::optional<U32> Read(U32 at) {if(at+4>bytes.size())return {};return Get<U32>(at);}
    void Write(U32 at,U32 v) {Put(at,v);}
    std::optional<U32> MemoryReadCode(U32 at) override {
        if(at==Stop)return 0xef000000;
        if(typing_testing && (at==0x2e1844 || at==0x2f6944 || at==0x3525d4))return 0xef000001;
        // Resource upload tail is outside this control-flow test. Keep the
        // native updater's prologue and epilogue, validation and save branches.
        if(room_testing && at==0x4286fc)return 0xea000000|((U32(0x428640)-at-8)>>2&0xffffff);
        if(at==0x33b5ec || at==0x33b5d0 || at==0x2e83ec || at==0x37547c ||
           at==0x2e7dc4 || at==0x2f2c54 || at==0x2f9430)return 0xef000001;
        if(save_testing && (at==0x2fdac8 || at==0x2fdaa4 || at==0x2fda7c || at==0x2e7d80 ||
           at==0x313ce0 || at==0x2f57f0 || at==0x2f4794 || at==0x2f9484))return 0xef000001;
        for(unsigned i=8;i<F::Sites.size();++i)if(at==F::Sites[i].pc)
            return F::Instruction(at,Get<U32>(at));
        return Get<U32>(at);
    }
    std::uint8_t MemoryRead8(U32 a) override {return Get<std::uint8_t>(a);}
    std::uint16_t MemoryRead16(U32 a) override {return Get<std::uint16_t>(a);}
    U32 MemoryRead32(U32 a) override {return Get<U32>(a);}
    std::uint64_t MemoryRead64(U32 a) override {return Get<std::uint64_t>(a);}
    void MemoryWrite8(U32 a,std::uint8_t v) override {Put(a,v);}
    void MemoryWrite16(U32 a,std::uint16_t v) override {Put(a,v);}
    void MemoryWrite32(U32 a,U32 v) override {Put(a,v);}
    void MemoryWrite64(U32 a,std::uint64_t v) override {Put(a,v);}
    void InterpreterFallback(U32,std::size_t) override {std::abort();}
    void ExceptionRaised(U32,Dynarmic::A32::Exception) override {std::abort();}
    void AddTicks(std::uint64_t n) override {ticks=n>ticks?0:ticks-n;}
    std::uint64_t GetTicksRemaining() override {return ticks;}
    void CallSVC(U32 svc) override {
        auto& r=cpu->Regs();
        if(!svc){cpu->HaltExecution();return;}
        if(svc>=F::FirstSVC+8 && svc<F::FirstSVC+F::Sites.size()) {
            if(svc>=F::FirstSVC+15)hook.KeyboardTyping(*this,r,svc-F::FirstSVC);
            else if(svc==F::FirstSVC+14)hook.NameDispatch(*this,r,true,now_us);
            else if(svc==F::FirstSVC+13)hook.KeyboardDelete(*this,r);
            else if(svc>=F::FirstSVC+11)F::KeyboardCursor(*this,r,cpu->ExtRegs(),svc-F::FirstSVC);
            else hook.KeyboardNavigation(*this,r,svc-F::FirstSVC);
            return;
        }
        assert(svc==1);
        const U32 pc=r[15]-4;
        if(typing_testing && pc==0x2e1844)Write(0x55be60+4*Read(K::Owner+0x24).value(),0xb10000);
        if(pc==0x2fdac8){++save_queues;assert(r[1]==1);}
        if(pc==0x2fdaa4)r[0]=save_done;
        if(pc==0x2fda7c){assert(save_done);++save_joins;}
        if(pc==0x313ce0 || pc==0x2f57f0)r[0]=0xb00000;
        if(pc==0x2f4794){++name_teardowns;Write(K::Owner+4,0);Write(F::Owner+0x10,1);}
        if(pc==0x2f9484){Write(r[0],0);Write(r[1],0);Write(r[2],0);}
        if(pc==0x33b5ec)r[0]=pressed;
        if(pc==0x2e7dc4){++validations;r[0]=valid_name;}
        if(pc==0x33b5d0)r[0]=directions;
        if(pc==0x37547c){sounds.push_back(r[0]);if(r[0]==0x010004be)++move_sounds;}
        if(pc==0x2f2c54) {
            std::array<float,12> quad;
            for(unsigned i=0;i<12;++i)quad[i]=Get<float>(r[1]+i*4);
            cursor_quads.push_back(quad);
        }
        if(pc==0x2f9430)cursor_origins.push_back({Get<float>(r[1]),Get<float>(r[1]+4)});
        r[15]=r[14];
    }
    U32 Call(U32 at,U32 arg=0) {
        auto& r=cpu->Regs();r.fill(0);r[0]=arg;r[13]=SP;r[14]=Stop;r[15]=at;
        for(unsigned i=4;i<=11;++i)r[i]=0xabc00000+i;
        cpu->SetCpsr(0x10);cpu->ClearHalt();ticks=10000;cpu->Run();
        assert(ticks && r[13]==SP);
        for(unsigned i=4;i<=11;++i)assert(r[i]==0xabc00000+i);
        return r[0];
    }
};
int main(int argc,char** argv) {
    assert(argc==3);Memory m;std::ifstream rom(argv[1],std::ios::binary|std::ios::ate);assert(rom);
    const auto length=rom.tellg();rom.seekg(0);rom.read(reinterpret_cast<char*>(m.bytes.data()+0x100000),length);
    Dynarmic::A32::UserConfig cfg;cfg.callbacks=&m;Dynarmic::A32::Jit cpu{cfg};m.cpu=&cpu;
    std::ifstream ff(std::string(argv[2])+"/message/us/ltn16.qbf",std::ios::binary);assert(ff);
    const std::vector<std::uint8_t> font_bytes{std::istreambuf_iterator<char>(ff),{}};
    const auto font=OOT3D::FileStatusText::MakeAssets(font_bytes);assert(font);
    std::atomic_store(&OOT3D::FileStatusText::assets,std::make_shared<const OOT3D::FileStatusText::Assets>(*font));
    m.Write(K::Owner+4,4);m.Write(K::Owner+0x40,1);
    F::Layout layout;layout.name_parts[F::Keyboard]={33,-12,.8f};
    layout.keyboard.spacing_x=8;layout.keyboard.spacing_y=6;
    F::SetLayout(layout,false);
    // Identical native page rectangles: custom page contents must not change padding.
    for(unsigned page=0;page<2;++page) {
        const U32 helper=0xa00000+page*0x100,positions=0xa01000+page*0x100,colors=0xa02000+page*0x100;
        m.Write(K::Owner+0x6c+4*page,helper);m.Write(helper,page?62:1);
        m.Write(helper+0x10,positions);m.Write(helper+0x18,colors);
        const std::array<float,12> quad{-2,34,0,322,34,0,-2,200,0,322,200,0};
        for(unsigned i=0;i<quad.size();++i)m.Put<float>(positions+i*4,quad[i]);
        m.Put<float>(colors+12,1);
    }
    std::vector<float> shadow_positions;

    for(unsigned mode:{2u,3u})for(unsigned caps=0;caps<3;++caps) {
        m.Write(K::Owner+8,mode);m.Write(K::Owner+0x20,caps);
        const auto table=m.Call(0x2e11e0,mode==3?7:4+caps);
        // The normal QWERTY character set is displayed alphabetically while
        // every displayed character keeps its original native insertion cell.
        const std::string alphabet="1234567890-=qwertyuiopasdfghjklzxcvbnm,./;' []";
        assert(alphabet.size()==46);
        for(unsigned i=0;i<64;++i) {
            unsigned code=mode==2 && i<alphabet.size()?static_cast<unsigned char>(alphabet[i]):0x100+i;
            if(mode==2 && caps && code>='a' && code<='z')code-=32;
            m.Put<std::uint16_t>(table+2*i,code);
        }
        auto keys=K::Keys(m,layout.keyboard);assert(keys.size()==(mode==2?51:57));
        if(mode==2) {
            // Real Shift replaces punctuation as well as letters. Controls
            // must retain the normal page's anchors and cursor/touch widths.
            const auto dash=table+2*alphabet.find('-'),comma=table+2*alphabet.find(',');
            m.Put<std::uint16_t>(dash,'_');m.Put<std::uint16_t>(comma,'<');
            const auto shifted=K::Keys(m,layout.keyboard);
            for(unsigned i=0;i<keys.size();++i)if(keys[i].command) {
                assert(shifted[i].display_x==keys[i].display_x && shifted[i].display_y==keys[i].display_y);
                assert(shifted[i].display_width==keys[i].display_width);
                assert(K::CursorSize(shifted[i])==K::CursorSize(keys[i]));
            }
            m.Put<std::uint16_t>(dash,'-');m.Put<std::uint16_t>(comma,',');
        }
        for(const auto& key:keys) {
            assert(key.text!=OOT3D::FileStatusText::Codepoints("Clr"));
            if(key.row==5)assert(key.display_x==160+11*(K::Columns-1) && key.display_y==181);
            if(key.row<0 || key.row==5)continue;
            m.Write(K::Owner+0x10,static_cast<U32>(key.column));m.Write(K::Owner+0x14,key.row);
            const auto index=m.Call(0x454acc);
            if(index>=1000)continue; // Native Caps, Shift, Delete, Clear.
            const auto code=m.Get<std::uint16_t>(table+index*2);
            assert(key.text.size()==1 && key.text[0]==code);
            const auto selected=K::Keys(m,layout.keyboard);
            unsigned selections=0;for(const auto& k:selected)selections+=k.selected;
            assert(selections==1);
        }
        m.Write(K::Owner+0x14,0);m.Write(K::Owner+0x10,mode==2?0:static_cast<U32>(-1));
        keys=K::Keys(m,layout.keyboard);
        if(mode==2) {
            const K::Key *caps_key=nullptr,*shift_key=nullptr,*dash=nullptr,*comma=nullptr;
            for(const auto& key:keys) {
                if(key.command && key.row==2)caps_key=&key;
                if(key.command && key.row==3)shift_key=&key;
                if(key.text==std::vector<unsigned>{'-'})dash=&key;
                if(key.text==std::vector<unsigned>{','})comma=&key;
            }
            assert(caps_key && shift_key && dash && comma);
            assert(shift_key->display_x==comma->display_x);
            const auto caps_text=K::Draw(*font,{*caps_key},layout.keyboard);
            const auto shift_text=K::Draw(*font,{*shift_key},layout.keyboard);
            const auto dash_text=K::Draw(*font,{*dash},layout.keyboard);
            assert(std::abs(caps_text.positions[0]-dash_text.positions[0])<.001f);
            const auto caps_bounds=F::Bounds(caps_text,0,caps_text.count-1);
            assert(shift_text.positions[0]-caps_bounds.right>6);
            assert(caps_bounds.left>=F::CustomKeyboardBounds(layout).left);
        }
        const auto shadow=F::NameShadow(m,layout,1);
        assert(shadow.count);
        const std::vector<float> positions(shadow.positions.begin(),shadow.positions.begin()+shadow.count*12);
        if(shadow_positions.empty())shadow_positions=positions;
        else assert(shadow_positions==positions); // Caps/Shift and accent pages share exact bounds.
        // Selection growth is presentation-only: no layout, neighbor or shadow changes.
        for(auto key:keys) {
            key.selected=true;
            auto options=layout.keyboard;
            const auto base=K::Draw(*font,{key},options);
            options.selected_grow=true;
            const auto grown=K::Draw(*font,{key},options);
            if(!key.command && key.text.size()==1 && key.text[0]!=32) {
                const auto a=F::Bounds(base,0,base.count-1),b=F::Bounds(grown,0,grown.count-1);
                assert(std::abs((b.right-b.left)/(a.right-a.left)-1.02f)<.0001f);
                assert(std::abs((b.bottom-b.top)/(a.bottom-a.top)-1.02f)<.0001f);
                key.selected=false;
                assert(K::Draw(*font,{key},options).positions==base.positions);
            } else assert(base.positions==grown.positions);
        }
        // Exercise the full presentation path with the game's row/column
        // selection, rather than setting Key::selected by hand. Moving focus
        // must resize only the new character and restore the old one.
        {
            auto rendering=layout;
            auto normal=F::KeyboardText(m,rendering,1);
            rendering.keyboard.selected_grow=true;
            for(float growth:{.02f,.045f,.2f,1.f}) {
                rendering.keyboard.selected_growth=growth;
                for(const auto& key:keys)if(!key.command && key.text[0]!=32) {
                    m.Write(K::Owner+0x14,key.row);m.Write(K::Owner+0x10,key.column);
                    const auto focused=K::Keys(m,rendering.keyboard);
                    const auto drawn=F::KeyboardText(m,rendering,1);
                    assert(drawn.count==normal.count);
                    unsigned q=0;
                    for(const auto& current:focused)for(unsigned code:current.text) {
                        if(!font->Find(code))continue;
                        const auto a=F::Bounds(normal,q,q),b=F::Bounds(drawn,q,q);
                        const float expected=current.selected?1+growth:1;
                        assert(std::abs((b.right-b.left)/(a.right-a.left)-expected)<.0001f);
                        assert(std::abs((b.bottom-b.top)/(a.bottom-a.top)-expected)<.0001f);
                        if(!current.selected)for(unsigned v=0;v<12;++v)
                            assert(normal.positions[q*12+v]==drawn.positions[q*12+v]);
                        ++q;
                    }
                }
            }
            m.Write(K::Owner+0x14,0);m.Write(K::Owner+0x10,mode==2?0:static_cast<U32>(-1));
        }
        {
            auto animated=layout;animated.keyboard.selected_grow=true;
            animated.keyboard.selected_growth=1;animated.keyboard.selected_growth_seconds=.1f;
            K::GrowthAnimation animation;
            const K::Key* first=nullptr;const K::Key* second=nullptr;
            unsigned first_quad=0,second_quad=0,q=0;
            for(const auto& key:keys) {
                if(K::Growable(key)) {
                    if(!first){first=&key;first_quad=q;}
                    else if(!second){second=&key;second_quad=q;}
                }
                for(auto code:key.text)q+=font->Find(code)!=nullptr;
            }
            assert(first && second);
            const auto base=F::KeyboardText(m,layout,1);
            const auto ratio=[&](const auto& g,unsigned quad) {
                const auto a=F::Bounds(base,quad,quad),b=F::Bounds(g,quad,quad);
                return (b.right-b.left)/(a.right-a.left);
            };
            m.Write(K::Owner+0x14,first->row);m.Write(K::Owner+0x10,first->column);
            float previous=1;
            for(unsigned frame=0;frame<6;++frame) {
                const auto g=F::KeyboardText(m,animated,1,&animation);
                const float scale=ratio(g,first_quad);
                assert(scale>previous && scale<=2.0001f);previous=scale;
                assert(std::abs(ratio(g,second_quad)-1)<.0001f);
            }
            assert(std::abs(previous-2)<.0001f);
            m.Write(K::Owner+0x14,second->row);m.Write(K::Owner+0x10,second->column);
            auto g=F::KeyboardText(m,animated,1,&animation);
            assert(ratio(g,first_quad)<2 && ratio(g,first_quad)>1);
            assert(ratio(g,second_quad)>1 && ratio(g,second_quad)<2);
            for(unsigned frame=0;frame<5;++frame)g=F::KeyboardText(m,animated,1,&animation);
            assert(std::abs(ratio(g,first_quad)-1)<.0001f && std::abs(ratio(g,second_quad)-2)<.0001f);
            animated.keyboard.selected_growth_seconds=0;
            m.Write(K::Owner+0x14,first->row);m.Write(K::Owner+0x10,first->column);
            g=F::KeyboardText(m,animated,1,&animation);
            assert(std::abs(ratio(g,first_quad)-2)<.0001f && std::abs(ratio(g,second_quad)-1)<.0001f);
            animated.keyboard.selected_grow=false;
            assert(F::KeyboardText(m,animated,1,&animation).positions==base.positions);
            F::KeyboardText(m,animated,0,&animation);assert(animation.amounts.empty());
            m.Write(K::Owner+0x14,0);m.Write(K::Owner+0x10,mode==2?0:static_cast<U32>(-1));
        }
        const auto before=m.bytes;
        auto g=K::Draw(*font,keys,layout.keyboard);assert(g.count && m.bytes==before);
        F::name_on_top=true;F::name_touch_ready=true;F::name_touch_confirmation=false;
        F::name_touch_keys=keys;F::touch_layout=layout;F::name_touch_progress=1;
        const auto t=F::NameTransform(layout,F::Keyboard);
        for(float width:{400.f,480.f})for(const auto& key:keys) {
            const auto c=K::Center(key,layout.keyboard);
            const float x=t.x-40+t.ax+(c[0]-t.ax)*t.sx;
            const float y=t.y+t.ay+(c[1]-t.ay)*t.sy;
            const auto touch=F::MapPointer((x+(width-320)/2)/width,y/240,width);
            if(key.row==-2)assert(!touch);
            else {assert(touch);assert(std::abs((*touch)[0]*320-key.X())<.001f && std::abs((*touch)[1]*240-key.Y())<.001f);}
        }
        // The old bottom stones must not remain invisible touch targets.
        for(float x:{52.f,268.f})assert(!F::MapPointer((x+40)/400,225.f/240,400));
        // Drawing and reverse hit-testing never rewrite tables/selection.
        assert(m.bytes==before);
        layout.keyboard.enabled=false;assert(F::CustomKeys(m,layout).empty());layout.keyboard.enabled=true;
        // Execute both ROM navigation functions, including the intercepted
        // repeat-input read, move sound and original stack/register epilogue.
        for(const auto& key:keys)if(key.row>=0)for(U32 direction:{0x10u,0x20u,0x40u,0x80u}) {
            m.Write(K::Owner+0x10,static_cast<U32>(key.column));m.Write(K::Owner+0x14,key.row);
            if(key.row==5)m.Write(K::Owner+0x28,key.column);
            m.Write(K::Owner+0x30,static_cast<U32>(-1));
            auto expected=K::Keys(m,layout.keyboard);
            const bool moved=K::Navigate(m,expected,direction);
            const auto col=m.Read(K::Owner+0x10),row=m.Read(K::Owner+0x14),button=m.Read(K::Owner+0x28);
            m.Write(K::Owner+0x10,static_cast<U32>(key.column));m.Write(K::Owner+0x14,key.row);
            if(key.row==5)m.Write(K::Owner+0x28,key.column);
            m.directions=direction;const auto sounds=m.move_sounds;
            m.Call(mode==2?0x43c8b8:0x43e3c0);
            assert(m.Read(K::Owner+0x10)==col && m.Read(K::Owner+0x14)==row);
            assert(row!=5 || button==1); // Hidden Cancel is never reachable.
            if(row==5)assert(m.Read(K::Owner+0x28)==button);
            assert(m.move_sounds==sounds+unsigned(moved));
        }
        if(mode==2) {
            const auto find=[&](unsigned c)->const K::Key& {
                for(const auto& k:keys)if(k.text.size()==1 && k.text[0]==c)return k;
                std::abort();
            };
            const auto& space=find(32);const auto& bracket=find(']');
            assert(space.display_x==bracket.display_x+22 && space.display_y==bracket.display_y);
            const unsigned a=caps?'A':'a';
            for(unsigned i=0;i<26;++i) {
                const auto& k=find(a+i);
                assert(k.display_x==160-11*(K::Columns-1)+22*(i%K::Columns) && k.display_y==77+26*(i/K::Columns));
                if(i>=24)continue; // Last two letters now share the numbers row.
                m.Write(K::Owner+0x10,static_cast<U32>(k.column));m.Write(K::Owner+0x14,k.row);
                const unsigned target=i<K::Columns?a+i+K::Columns:a+i-K::Columns;
                K::Navigate(m,K::Keys(m,layout.keyboard),i<K::Columns?0x80:0x40);
                const auto& other=find(target);
                assert(m.Read(K::Owner+0x10)==static_cast<U32>(other.column) && m.Read(K::Owner+0x14)==static_cast<U32>(other.row));
            }
        }
        // Execute the native eight-corner builder for every key, including
        // Caps/Shift/blank space/OK and tabs. Each art quad stays 16x16; only
        // separation changes. The native pulse/UV/color paths are untouched.
        constexpr U32 cursor=0x900000;
        m.Write(K::Owner,cursor);m.Write(cursor+0xc,8);
        for(const auto& key:keys)if(key.row!=-2) {
            m.Write(K::Owner+0x10,static_cast<U32>(key.column));m.Write(K::Owner+0x14,key.row);
            if(key.row==5)m.Write(K::Owner+0x28,key.column);
            m.cursor_quads.clear();m.cursor_origins.clear();
            cpu.ExtRegs()[0]=std::bit_cast<U32>(120.f);cpu.ExtRegs()[1]=std::bit_cast<U32>(42.f);
            m.Call(0x2f79b4,cursor);m.Call(0x2f7af4,cursor);
            assert(m.cursor_quads.size()==8 && m.cursor_origins.size()==8);
            const auto [cw,ch]=K::CursorSize(key);
            for(unsigned i=0;i<8;++i) {
                const auto& q=m.cursor_quads[i];
                assert(q[3]-q[0]==16 && q[7]-q[1]==16);
                assert(std::abs(q[0]-(i%2?(cw-16):0))<.001f);
                assert(std::abs(q[1]-((i%4)/2?ch-16:0))<.001f);
                assert(std::abs(m.cursor_origins[i][0]-(key.X()-cw/2))<.001f);
                assert(std::abs(m.cursor_origins[i][1]-(key.Y()-ch/2))<.001f);
            }
        }
        // Grid OK uses the native submit branch, including both empty-name
        // rejection and the existing validation gate. No character is inserted.
        m.Write(K::Owner+0x14,5);m.Write(K::Owner+0x28,1);
        m.Write(K::Owner+0x30,static_cast<U32>(-1));m.pressed=1;m.directions=0;
        const auto input=mode==2?0x43c8b8:0x43e3c0;
        for(unsigned length:{0u,3u})for(bool valid:{false,true}) {
            m.Write(K::Owner+4,4);m.Write(K::Owner+0x24,length);m.valid_name=valid;
            const auto validations=m.validations;
            m.Call(input);
            assert(m.validations==validations+(length!=0));
            assert(m.Read(K::Owner+4)==(length && valid?5u:4u));
            assert(m.Read(K::Owner+0x24)==length);
        }
        m.pressed=0;m.Write(K::Owner+4,4);
        // Both native B paths delete one character and emit the same single cue.
        // Execute the real deletion routine, with no live text models to destroy.
        for(unsigned length:{1u,4u,7u,8u}) {
            m.Write(K::Owner+0x24,length);m.Write(K::Owner+0x30,static_cast<U32>(-1));
            for(unsigned i=0;i<8;++i)m.Write(0x55be60+4*i,0);
            for(unsigned i=0;i<10;++i)m.Put<std::uint16_t>(0x55be4c+2*i,'a'+i);
            m.pressed=2;m.sounds.clear();m.Call(input);
            assert(m.Read(K::Owner+0x24)==length-1);
            assert(m.sounds==std::vector<U32>{0x010004bf});
            assert(m.Read(K::Owner+4)==4 && !m.hook.keyboard_delete_pending);
        }
        m.pressed=0;

        // Other native cursors keep their supplied dimensions.
        m.cursor_quads.clear();m.Write(cursor+0x100+0xc,8);
        cpu.ExtRegs()[0]=std::bit_cast<U32>(120.f);cpu.ExtRegs()[1]=std::bit_cast<U32>(42.f);
        m.Call(0x2f79b4,cursor+0x100);
        assert(m.cursor_quads[1][0]==120 && m.cursor_quads[2][1]==42);
    }
    // Run the original save updater. Pending async I/O cannot be mistaken for
    // completion; auto-return is allowed only after its native join and mode 6.
    m.save_testing=true;m.Write(0x55a21c,1);
    for(unsigned slot=0;slot<3;++slot) {
        m.hook.name_save={};m.save_done=false;m.save_queues=m.save_joins=m.name_teardowns=0;
        m.Write(K::Owner+4,5);m.Write(K::Owner+8,2);m.Write(K::Owner+0x24,4);
        assert(m.Read(0x43047c)==0x588958);
        m.Write(m.Read(0x43047c).value()+0x4dc,slot);
        m.Write(OOT3D::MasterQuest::System+0x4dc,3); // Deliberately invalid decoy.
        std::array<U32,16> r{};r[6]=K::Owner;r[15]=F::Sites[14].pc+4;
        m.hook.NameDispatch(m,r,true);assert(m.hook.name_save.active);
        assert(m.hook.saving_keyboard && m.hook.saving_keyboard->text.count);
        const auto frozen_text=m.hook.saving_keyboard->text.positions;
        m.Write(K::Owner+4,4);m.Write(K::Owner+8,5);m.Write(K::Owner+0x3c,0);
        for(unsigned frame=0;frame<12;++frame) {
            m.Call(0x43e068);
            assert(m.Read(K::Owner+8)==5 && m.save_joins==0 && m.name_teardowns==0);
        }
        assert(m.save_queues==1);
        assert(F::KeyboardText(m,F::GetLayout(),1).count==0); // Native mode has left the keyboard.
        assert(m.hook.saving_keyboard->text.positions==frozen_text); // Presentation retains it.
        m.save_done=true;m.Call(0x43e068);
        assert(m.Read(K::Owner+8)==6 && m.save_joins==1 && m.save_queues==1);
        m.Call(0x427cc0); // Actual prologue, hook redirect and native epilogue.
        assert(m.name_teardowns==1 && m.Read(K::Owner+4)==0);
        assert(m.hook.name_save.returning && m.hook.name_save.slot==slot);
        m.Write(F::Owner+0x10,2);m.hook.name_save.FileDispatch(m,r);
        assert(m.Read(F::Owner+0x28)==10);
        m.Write(F::Owner+0x10,3);m.Write(m.Read(0x42fdfc).value()+4*slot,1);
        assert(m.hook.name_save.FileDispatch(m,r));
        assert(r[15]==0x43006c && m.Read(F::Owner+0x14)==slot);
        m.Write(F::Owner+0x10,7);assert(!m.hook.name_save.FileDispatch(m,r));
        assert(m.hook.name_save.returning); // Do not expose the unfinished action page.
        for(unsigned i=0;i<3;++i) {
            assert(std::bit_cast<float>(m.Read(0x55b5f8+4*i).value())==(i==slot?16.f:-384.f));
            assert(std::bit_cast<float>(m.Read(0x55b604+4*i).value())==(i==slot?56.f:48.f+52*i));
            assert(m.Read(0x55b610+4*i)==0 && m.Read(0x55b61c+4*i)==0);
        }
        m.Write(F::Owner+0x10,8);assert(m.hook.name_save.FileDispatch(m,r));
        assert(!m.hook.name_save.returning && m.hook.name_save.entering);
        assert(r[15]==0x4336f8);
        for(unsigned frame=0;frame<F::NameSaveFlow::EntranceFrames;++frame) {
            assert(m.hook.name_save.FileDispatch(m,r));
            m.hook.name_save.Frame();
        }
        assert(!m.hook.name_save.FileDispatch(m,r) && !m.hook.name_save.entering);
    }
    {
        namespace T=OOT3D::NameTyping;
        m.typing_testing=true;cpu.ClearCache();m.hook.name_save={};
        m.pressed=m.directions=0;m.valid_name=true;
        const std::string letters="1234567890-=qwertyuiopasdfghjklzxcvbnm,./;' []";
        for(unsigned caps=0;caps<3;++caps)for(unsigned i=0;i<letters.size();++i) {
            unsigned code=letters[i];if(caps && code>='a' && code<='z')code-=32;
            m.Put<std::uint16_t>(0x55b64e + caps*0x200+2*i,code);
        }
        m.Put<std::uint16_t>(0x55ba4e + 20,'_');
        m.Put<std::uint16_t>(0x55bc4e,0xe9);
        const auto reset=[&] {
            m.Write(K::Owner+4,4);m.Write(K::Owner+8,2);m.Write(K::Owner+0x20,0);
            m.Write(K::Owner+0x24,0);m.Write(K::Owner+0x4c,1);m.Write(K::Owner+0x40,1);
            for(unsigned i=0;i<8;++i){m.Put<std::uint16_t>(0x55be4c+2*i,0);m.Write(0x55be60+4*i,0);}
            T::Publish(true);
        };
        const auto type=[&](unsigned code) {
            T::Push(code);m.hook.typing.Frame(m,true);
            const auto mode=m.Read(K::Owner+8).value();
            m.Call(mode==2?0x43c8b8:0x43e3c0);
        };
        reset();
        for(unsigned code:{'L','i','n','k','1','_'}) {
            const auto pos=m.Read(K::Owner+0x24).value();
            type(code);
            assert(m.Get<std::uint16_t>(0x55be4c+2*pos)==code);
            bool selected=false;
            for(const auto& key:K::Keys(m))if(key.selected && key.text==std::vector<unsigned>{code})selected=true;
            // Native one-shot Shift returns to lowercase after insertion.
            assert(selected || code=='_');
        }
        type(0xe9);assert(m.Read(K::Owner+8)==3 && m.Get<std::uint16_t>(0x55be4c+12)==0xe9);
        type(T::Backspace);assert(m.Read(K::Owner+0x24)==6 && m.Read(0x55be60+24)==0 && m.Read(K::Owner+4)==4);
        type('a');assert(m.Read(K::Owner+8)==2 && m.Get<std::uint16_t>(0x55be4c+12)=='a');
        const auto before=m.Get<std::uint64_t>(0x55be4c);
        type(0x1f600);assert(m.Get<std::uint64_t>(0x55be4c)==before); // Unsupported glyph ignored.
        type(T::Confirm);assert(m.Read(K::Owner+4)==5);
        reset();type(T::Backspace);assert(m.Read(K::Owner+4)==4 && m.Read(K::Owner+0x24)==0);
        type(T::Confirm);assert(m.Read(K::Owner+4)==4); // Empty name rejected by the native OK path.
        T::Push('a');m.Write(K::Owner+4,5);m.hook.typing.Frame(m,true);
        assert(!T::available && !T::Pop() && !m.hook.typing.buttons);
        m.typing_testing=false;cpu.ClearCache();
    }
    // Session-name acceptance must take the fade/cleanup path on BOTH keyboard
    // pages, with zero save jobs and no changes to any native save bytes.
    namespace J=OOT3D::RoomJoin;
    for(unsigned mode:{2u,3u}) {
        J::Begin(1,0);m.hook.name_save={};m.room_testing=true;cpu.ClearCache();
        m.save_queues=m.save_joins=m.name_teardowns=0;m.now_us=1000000;
        m.Write(K::Owner+4,5);m.Write(K::Owner+8,mode);m.Write(K::Owner+0x24,4);
        const std::u16string session=u"Ren\u00e9";
        for(unsigned i=0;i<session.size();++i)m.Put<std::uint16_t>(0x55be4c+2*i,session[i]);
        const std::vector<unsigned char> saves(m.bytes.begin()+0x55bec0,m.bytes.begin()+0x55bec0+3*0x15c4);
        const std::vector<unsigned char> live(m.bytes.begin()+0x587958,m.bytes.begin()+0x587958+0x14dc);
        m.Call(0x427cc0);
        assert(J::phase==J::Phase::NameLeaving && J::opacity==0 && J::name==session);
        assert(!m.hook.name_save.active && m.hook.saving_keyboard);
        m.now_us+=75000;m.Call(0x427cc0);
        assert(J::opacity==.5f && m.name_teardowns==0);
        m.now_us+=75000;m.Call(0x427cc0);
        assert(J::phase==J::Phase::ReturnToFiles && m.name_teardowns==1);
        assert(m.save_queues==0 && m.save_joins==0 && m.Read(K::Owner+4)==0);
        assert(std::equal(saves.begin(),saves.end(),m.bytes.begin()+0x55bec0));
        assert(std::equal(live.begin(),live.end(),m.bytes.begin()+0x587958));
        J::Reset();m.room_testing=false;cpu.ClearCache();
    }
    m.save_testing=false;
    m.Write(K::Owner+0x40,0);assert(K::Keys(m,layout.keyboard).empty());
    m.Write(K::Owner+0x40,1);m.Write(K::Owner+8,5);assert(K::Keys(m,layout.keyboard).empty());
    {
        F::NamePromptTransition fade;
        for(unsigned i=0;i<12;++i)fade.Frame(true,false,1);
        assert(fade.keyboard==1 && fade.standard==0);
        // Either keyboard page remains fully lit. Native teardown may finish
        // before the outgoing labels: the transition must survive that handoff.
        fade.Frame(true,false,1);assert(fade.keyboard==1);
        float previous=1;
        for(unsigned i=0;i<9;++i) {
            fade.Frame(false,true,1);
            assert(fade.keyboard<=previous && (fade.keyboard==0 || fade.standard==0));
            previous=fade.keyboard;
        }
        for(unsigned i=0;i<12;++i)fade.Frame(false,true,1);
        assert(fade.keyboard==0 && fade.standard==1);
        fade.Frame(true,false,.1f);assert(fade.standard==0 && fade.keyboard<=.1f);
        fade.Frame(false,false,1,true);assert(fade.keyboard==0 && fade.standard==0);
    }
    std::cout<<"Alphabetical keyboard grid, native character/caps/accent mapping, touch, ARM navigation, register preservation and prompt fades passed\n";
}
