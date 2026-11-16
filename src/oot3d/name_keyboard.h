// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/file_status_assets.h"

namespace OOT3D::NameKeyboard {
using U32=std::uint32_t;
constexpr U32 Owner=0x5077f0;
constexpr unsigned Columns=12;
struct Settings {
    bool enabled=true;
    float spacing_x=0,spacing_y=0,font_scale=1;
    bool selected_grow=false;
    float selected_growth=.02f;
    float selected_growth_seconds=.1f;
    float cursor_x=0,cursor_y=0;
};
struct Key {
    int row=0,column=0;
    float x=0,y=0,w=26,h=26;
    std::vector<unsigned> text;
    bool selected=false,active=true;
    float display_x=0,display_y=0,display_width=22;
    bool command=false;
    float growth_amount=-1; // Presentation snapshot; negative means immediate selection.
    float X() const {return x+w/2;}
    float Y() const {return y+h/2;}
};
inline float Spacing(float v) {return std::isfinite(v)?std::clamp(v,-10.f,24.f):0;}
inline std::array<float,2> Center(const Key& key,const Settings& s) {
    return {160+(key.display_x-160)*(1+Spacing(s.spacing_x)/26),
            117+(key.display_y-117)*(1+Spacing(s.spacing_y)/26)};
}
// Keep native cell identities for insertion, tabs, caps/shift and touch. Only
// their presentation and directional navigation use the alphabetical grid.
// Characters still come from the live UTF-16 tables used by the game.
template<class M> std::vector<Key> Keys(M& m,const Settings& settings={},unsigned page=~0u,unsigned letter_case=~0u) {
    if(!m.Read(Owner+0x40).value_or(0))return {};
    const auto mode=page==~0u?m.Read(Owner+8).value_or(0):page;
    const auto caps=letter_case==~0u?m.Read(Owner+0x20).value_or(0):letter_case;
    if((mode!=2 && mode!=3) || caps>2)return {};
    const U32 table=mode==3?0x55bc4e:0x55b64e + caps*0x200;
    // Tables are populated at name-entry initialization, not ROM startup.
    const auto first=m.Read(table&~3u);
    if(!first || !(*first>>16))return {};
    const int row=static_cast<std::int32_t>(m.Read(Owner+0x14).value_or(0));
    const int col=static_cast<std::int32_t>(m.Read(Owner+0x10).value_or(0));
    std::vector<Key> out;
    const auto label=[&](int r,int c,float x,float y,float w,float h,std::string_view text,bool active=true) {
        out.push_back({r,c,x,y,w,h,FileStatusText::Codepoints(text),r==row && (r==-1 || c==col),active});
        out.back().command=true;
    };
    const auto glyph=[&](int r,int c,int index,float x,float y,float w=26.f) {
        const U32 at=table+index*2;
        const auto word=m.Read(at&~3u);if(!word)return false;
        const unsigned code=(*word>>((at&2)*8))&0xffff;
        if(code<32 || code==0xffff)return false;
        Key key{r,c,x,y,w,26,{code},r==row && c==col};
        out.push_back(std::move(key));return true;
    };
    // Native tab focus always selects the OTHER page. The current tab stays
    // visible but cannot insert characters or change the native input rules.
    label(mode==3?-1:-2,0,30,38,128,24,"ABC/123",mode==2);
    label(mode==2?-1:-2,0,160,38,130,24,"À Á",mode==3);
    if(mode==2) {
        for(int c=0;c<12;++c)if(!glyph(0,c,c,4+26*c,64))return {};
        for(int c=0;c<10;++c)if(!glyph(1,c,12+c,16+26*c,90))return {};
        label(2,-1,4,116,26,26,"Caps",caps==1);
        for(int c=0;c<9;++c)if(!glyph(2,c,22+c,30+26*c,116))return {};
        label(3,-1,4,142,38,26,"Shift",caps==2);
        for(int c=0;c<10;++c)if(!glyph(3,c,31+c,42+26*c,142))return {};
        for(int c=0;c<2;++c)if(!glyph(4,c,41+c,56+26*c,168))return {};
        if(!glyph(4,2,43,108,168,130) || !glyph(4,3,44,238,168) ||
           !glyph(4,4,45,264,168))return {};
    } else {
        for(int r=0;r<5;++r)for(int c=-1;c<=(r==4?8:9);++c)
            if(!glyph(r,c,r*11+c+1,4+26*(c+1),64+26*r))return {};
    }
    // Reuse native OK (including validation), but expose it only in the grid.
    label(5,1,218,202,100,36,"OK");
    out.back().selected=row==5 && m.Read(Owner+0x28)==1;
    unsigned letter=0,symbol=0,accent=0;
    std::vector<Key*> letters;
    for(auto& key:out)if(mode==2 && key.text.size()==1 &&
        ((key.text[0]>='a' && key.text[0]<='z') || (key.text[0]>='A' && key.text[0]<='Z')))
        letters.push_back(&key);
    std::sort(letters.begin(),letters.end(),[](const Key* a,const Key* b){
        return (a->text[0]|32)<(b->text[0]|32);
    });
    const auto cell=[](Key& key,unsigned r,unsigned c) {
        key.display_x=160-11*(Columns-1)+22*c;key.display_y=77+26*r;
    };
    for(auto* key:letters){cell(*key,letter/Columns,letter%Columns);++letter;}
    for(auto& key:out) {
        if(key.row<0) {
            key.display_x=key.X();key.display_y=key.Y();key.display_width=key.w;continue;
        }
        if(std::find(letters.begin(),letters.end(),&key)!=letters.end())continue;
        if(mode==2 && key.text==std::vector<unsigned>{32}) {
            // Space is the blank cell immediately after the closing bracket.
            cell(key,45/Columns,45%Columns);continue;
        }
        if(key.row==5) {
            key.display_x=160+11*(Columns-1);key.display_y=181;key.display_width=60;continue;
        }
        const bool control=key.command || key.text.size()!=1;
        if(mode==2 && control) {
            // Group typing controls at the left; OK keeps its right-hand anchor.
            const float first_column=160-11*(Columns-1);
            key.display_x=first_column+(key.text==FileStatusText::Codepoints("Caps")?0.f:66.f);
            key.display_y=181;key.display_width=60;
        } else if(mode==2) {
            const unsigned index=letter+symbol++;
            cell(key,index/Columns,index%Columns);
        } else {cell(key,accent/Columns,accent%Columns);++accent;}
    }
    if(mode==2)if(const auto font=FileStatusText::GetAssets()) {
        // Anchor to the unshifted dash/comma CELLS. Shift replaces those
        // glyphs with underscore/<, but must not move or resize the controls.
        // Measure words before tightening their cursor/touch bounds.
        const auto span=[&](const Key& key) {
            float width=0;for(auto c:key.text)if(const auto g=font->Find(c))width+=g->width+1;
            const float size=std::isfinite(settings.font_scale)?std::clamp(settings.font_scale,.4f,2.f):1.f;
            return width>0?std::min(size,(key.display_width-4)/width)*width:0.f;
        };
        Key* caps_key=nullptr;Key* shift_key=nullptr;
        for(auto& key:out) {
            if(key.command && key.row==2)caps_key=&key;
            if(key.command && key.row==3)shift_key=&key;
        }
        if(caps_key && shift_key) {
            Key dash,comma;cell(dash,3,0);cell(comma,3,2);dash.text={'-'};
            const float spacing=1+Spacing(settings.spacing_x)/26;
            float caps_width=span(*caps_key),shift_width=span(*shift_key);
            const float left=Center(dash,settings)[0]-span(dash)/2;
            // Tight column settings must not make the two words overlap.
            const float fit=std::min(1.f,(Center(comma,settings)[0]-left-8)/(caps_width+shift_width/2));
            caps_width*=fit;shift_width*=fit;
            caps_key->display_x=160+(left+caps_width/2-160)/spacing;
            shift_key->display_x=comma.display_x;
            caps_key->display_width=caps_width+4;
            shift_key->display_width=shift_width+4;
        }
    }
    return out;
}
// Use the game's repeat-input clock but choose the adjacent DISPLAYED cell.
// Native A/B, allowed characters, validation and tab handling remain intact.
template<class M> bool Navigate(M& m,std::vector<Key> keys,U32 buttons) {
    buttons&=0xf0;if(!buttons || keys.empty())return false;
    const Key* current=nullptr;
    for(const auto& k:keys)if(k.selected){current=&k;break;}
    if(!current)return false;
    const bool horizontal=buttons&0x30;
    const float sign=buttons&(horizontal?0x10:0x80)?1.f:-1.f;
    const Key* next=nullptr;float best=INFINITY;
    for(const auto& k:keys) {
        if(k.row==-2 || &k==current)continue;
        const float along=(horizontal?k.display_x-current->display_x:k.display_y-current->display_y)*sign;
        const float cross=std::abs(horizontal?k.display_y-current->display_y:k.display_x-current->display_x);
        if((horizontal && cross>.1f) || along<=.1f)continue;
        const float score=along*1000+cross;
        if(score<best){best=score;next=&k;}
    }
    if(!next) { // Wrap at the edge of a row or from the buttons to the tabs.
        best=INFINITY;
        for(const auto& k:keys) {
            if(k.row==-2 || &k==current)continue;
            const float cross=std::abs(horizontal?k.display_y-current->display_y:k.display_x-current->display_x);
            if(horizontal && cross>.1f)continue;
            const float score=(horizontal?k.display_x:k.display_y)*sign*1000+cross;
            if(score<best){best=score;next=&k;}
        }
    }
    if(!next)return false;
    m.Write(Owner+0x14,static_cast<U32>(next->row));
    m.Write(Owner+0x10,static_cast<U32>(next->column));
    if(next->row==5)m.Write(Owner+0x28,static_cast<U32>(next->column));
    return true;
}
inline std::array<float,2> CursorSize(const Key& key) {
    // Keep the original corner art at 16x16; separate word corners far enough
    // to clear the label rather than stretching the art across it.
    return {key.display_width+(key.command?17.f:5.f),key.command?35.f:31.f};
}
inline bool Growable(const Key& key) {
    return !key.command && key.text.size()==1 && key.text[0]!=32;
}
struct GrowthAnimation {
    std::vector<Key> identity;
    std::vector<float> amounts;
    void Reset() {identity.clear();amounts.clear();}
    void Apply(std::vector<Key>& keys,const Settings& s) {
        bool same=identity.size()==keys.size();
        for(unsigned i=0;same && i<keys.size();++i)
            same=identity[i].row==keys[i].row && identity[i].column==keys[i].column && identity[i].text==keys[i].text;
        // Page/case changes must not transfer a previous letter's growth to a new key.
        if(!same) {identity=keys;amounts.assign(keys.size(),0);}
        const float seconds=std::isfinite(s.selected_growth_seconds)?std::clamp(s.selected_growth_seconds,0.f,.5f):.1f;
        const float step=seconds>0?1/(60*seconds):1;
        for(unsigned i=0;i<keys.size();++i) {
            if(!s.enabled || !s.selected_grow || !Growable(keys[i]))amounts[i]=0;
            else amounts[i]=std::clamp(amounts[i]+(keys[i].selected?step:-step),0.f,1.f);
            const float t=amounts[i];
            keys[i].growth_amount=t*t*(3-2*t);
        }
    }
};
inline FileStatusText::Geometry Draw(const FileStatusText::Assets& font,
        const std::vector<Key>& keys,const Settings& s) {
    FileStatusText::Geometry out;
    for(const auto& key:keys) {
        const auto center=Center(key,s);
        float size=std::isfinite(s.font_scale)?std::clamp(s.font_scale,.4f,2.f):1.f;
        float width=0;for(auto code:key.text)if(const auto g=font.Find(code))width+=g->width+1;
        if(!width)continue;
        size=std::min(size,(key.display_width-4)/width);
        if(s.selected_grow && Growable(key)) {
            const float amount=key.growth_amount<0?(key.selected?1.f:0.f):std::clamp(key.growth_amount,0.f,1.f);
            size*=1+amount*(std::isfinite(s.selected_growth)?std::clamp(s.selected_growth,0.f,1.f):.02f);
        }
        // The inverted punctuation in the accented last row extends below the
        // normal baseline. Lower OK slightly for visual alignment on both pages.
        const float text_y=center[1]+(key.row==5?1.5f*size:0);
        float x=center[0]-width*size/2;
        for(auto code:key.text)if(const auto g=font.Find(code)) {
            out.Add(x,text_y-10*size,(g->width+2)*size,20*size,
                float(g->x)/FileStatusText::Width,(g->y+.5f)/FileStatusText::Height,
                float(g->x+g->width+2)/FileStatusText::Width,(g->y+19.5f)/FileStatusText::Height);
            const float light=key.selected?1.f:key.active?.9f:.65f;
            for(unsigned v=0;v<4;++v)for(unsigned c=0;c<3;++c)
                out.colors[(out.count-1)*16+v*4+c]=key.selected && c==2?.4f:light;
            x+=(g->width+1)*size;
        }
    }
    return out;
}
}
