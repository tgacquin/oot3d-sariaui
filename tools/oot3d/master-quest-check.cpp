// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include "oot3d/randomizer/config.h"
#include "oot3d/room_join_chime.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace Q=OOT3D::MasterQuest;
using Q::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {
        auto it=words.find(a);return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void Allocate(U32 a,U32 size) {for(U32 i=0;i<size;i+=4)words[a+i]=0;}
};
std::vector<std::uint8_t> Read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
unsigned CRC(Memory& m,U32 at) {
    unsigned crc=0;
    for(U32 i=0;i<Q::SaveSize;++i) {
        crc^=Q::Byte(m,at+i);
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xa001:0);
    }
    return crc;
}
struct ToggleFiles : Q::OrdinaryFiles {
    unsigned* toggles;
    void ToggleRandomizer() {++*toggles;}
};
int main(int argc,char** argv) {
    assert(argc==3);Memory m;
    auto binary=Read(argv[1]);
    for(unsigned i=0;i+3<binary.size();i+=4)m.words[0x100000+i]=OOT3D::MapFrame::LE32(binary,i);
    assert(Q::CodeMatches(m));
    {
        namespace A=OOT3D::Randomizer::Activity;
        namespace C=OOT3D::RoomJoinChime;
        assert(C::CodeMatches(m));
        C::Hook sound;std::array<U32,16> regs{};std::array<U32,64> fp{};
        for(unsigned i=0;i<16;++i)regs[i]=i+10;
        for(unsigned i=0;i<64;++i)fp[i]=i+100;
        regs[13]=0x09000100;regs[15]=0x42f9d4;
        m.Allocate(regs[13]-8,8);m.Write(regs[13]-8,0xabc);m.Write(regs[13]-4,0xdef);
        const auto original=regs;const auto original_fp=fp;
        U32 flags=0x60000010,fpscr=0x03000000;
        A::Connection(true);A::Presence("Guest",true);
        assert(sound.Frame(m,regs,fp,flags,fpscr,0x42f9d0,1,0));
        assert(regs[0]==C::Sound && regs[15]==C::Play && regs[14]==0x42f9d0);
        assert(regs[13]==original[13]-8 && m.Read(regs[13])==0x1148f8);
        assert(!A::TakeJoinChime()); // Exactly one sound per arrival.
        const U32 sp=regs[13];regs.fill(0);regs[13]=sp;fp.fill(0);flags=fpscr=0;
        assert(!sound.Frame(m,regs,fp,flags,fpscr,0x42f9d0,1,100));
        assert(regs==original && fp==original_fp && flags==0x60000010 && fpscr==0x03000000);
        assert(m.Read(sp)==0xabc && m.Read(sp+4)==0xdef);
        A::Presence("Guest",false);assert(!A::TakeJoinChime());
        A::Presence("Next",true);A::Connection(false);assert(!A::TakeJoinChime());
        A::Presence("Late",true);assert(!A::TakeJoinChime());
        A::SetGame("chime-test");A::SetGame({});
    }
    for(unsigned i=0;i<Q::Sites.size();++i) {
        auto s=Q::Sites[i];assert(Q::Instruction(s.pc,s.instruction)==(0xef000000|(Q::FirstSVC+i)));
        m.words[s.pc]^=1;assert(!Q::CodeMatches(m));m.words[s.pc]^=1;
    }
    // Verify the save-bank query has exactly the seven known direct callers.
    unsigned callers=0;
    for(unsigned i=0;i+4<=binary.size();i+=4) {
        const U32 word=OOT3D::MapFrame::LE32(binary,i),pc=0x100000+i;
        if((word&0xff000000)!=0xeb000000)continue;
        const auto disp=std::int32_t(word<<8)>>6;
        if(U32(pc+8+disp)==0x2e63c8)++callers;
    }
    assert(callers==7);
    auto texture=Read(std::string(argv[2])+"/misc/us/english/ura.ctxb");
    const auto image=OOT3D::QuestChoice::MakePixels(texture);assert(image);
    namespace Status=OOT3D::FileStatusText;
    const auto font=Status::MakeAssets(Read(std::string(argv[2])+"/message/us/ltn16.qbf"));assert(font);
    for(const auto label:{"Randomizer on","Randomizer off","Seed: 12345"}) {
        const auto text=Status::Text(*font,label,0,218,104);
        assert(text.count==std::string_view(label).size());
        for(unsigned i=0;i<text.count*8;++i)assert(text.uvs[i]>=0 && text.uvs[i]<=1);
        for(unsigned i=0;i<text.count*12;i+=3)assert(text.positions[i]>=0 && text.positions[i]<=106);
    }
    assert(Status::Codepoints("caf\xc3\xa9")==std::vector<unsigned>({'c','a','f',0xe9}));
    const auto long_seed=Status::Text(*font,"Seed: "+std::string(128,'W'),-3,218,406);
    assert(long_seed.count==134);
    for(unsigned i=0;i<long_seed.count*12;i+=3) {
        assert(long_seed.positions[i]>=-3 && long_seed.positions[i]<=405);
        assert(long_seed.positions[i+1]>150 && long_seed.positions[i+1]<=218);
    }
    assert(std::memcmp(image->data(),texture.data()+72,texture.size()-72)==0);
    texture[48]^=1;assert(!OOT3D::QuestChoice::MakePixels(texture));
    // The shared prompt uses the native message renderer, switching its ID
    // only while choosing a mode; B immediately restores the file prompt.
    Q::Hook label_hook;std::array<U32,16> label_regs{};
    Q::choosing=false;label_hook.Handle(m,label_regs,7);assert(label_regs[1]==0x950);
    Q::choosing=true;label_hook.Handle(m,label_regs,7);assert(label_regs[1]==0x900);
    Q::cancelling=true;label_hook.Handle(m,label_regs,7);assert(label_regs[1]==0x950);
    Q::cancelling=false;label_hook.Handle(m,label_regs,7);
    label_regs[0]=0x8000000;label_regs[1]=0x55b548;
    m.words[0x55b560]=0;
    label_hook.Handle(m,label_regs,8);
    assert(m.Read(0x55b560)==0x8000000 && Q::prompt_controller==0x8000000 && Q::prompt_message==0x900);
    label_hook.Reset();
    for(unsigned i=0;i<2;++i) {
        auto g=OOT3D::FileSelect::QuestStones(i);assert(g.count==12);
        for(float uv:g.uvs)assert(uv>=0 && uv<=1);
        assert(std::abs(g.positions[1*12+3]-g.positions[1*12]-124.2f*.75f)<.001f);
        // Native bottom-origin (8,64,138,184) corresponds to stored rows 8..191.
        assert(g.uvs[8+1]==247.5f/256 && g.uvs[8+5]==64.5f/256);
    }
    // Each stone, including its selection corners, uses its own stable center.
    OOT3D::FileSelect::Layout base,edited;base.quest_stones={};
    edited.quest_stones={{{11,17,.5f},{-50,20,1.25f}}};edited.quest_description={-70,90,.6f};
    const auto a=OOT3D::FileSelect::QuestStones(1,{},base);
    const auto b=OOT3D::FileSelect::QuestStones(1,{},edited);
    for(unsigned q=0;q<12;++q)for(unsigned v=0;v<4;++v) {
        const unsigned mode=q<4?q/2:1;const auto p=edited.quest_stones[mode];
        const float cx=124.1f+154*mode;
        assert(std::abs(b.positions[q*12+v*3]-(cx+p.x+(a.positions[q*12+v*3]-cx)*p.scale))<.001f);
        assert(std::abs(b.positions[q*12+v*3+1]-(122.8f+p.y+(a.positions[q*12+v*3+1]-122.8f)*p.scale))<.001f);
    }
    // Highlight overlays pulse without moving/scaling the opaque corners or
    // stones. Both layers inherit the same per-stone transform and slide.
    Q::cursor_us=0;const auto dark=OOT3D::FileSelect::QuestStones(1,{},edited);
    Q::cursor_us=1000000;const auto bright=OOT3D::FileSelect::QuestStones(1,{},edited);
    assert(dark.positions==bright.positions && dark.uvs==bright.uvs);
    for(unsigned q=0;q<12;++q)for(unsigned v=0;v<4;++v) {
        const unsigned alpha=q*16+v*4+3;
        if(q<8)assert(dark.colors[alpha]==bright.colors[alpha]);
        else {
            assert(dark.colors[alpha]==0 && bright.colors[alpha]==180.f/255.f);
            for(unsigned axis=0;axis<3;++axis)
                assert(bright.positions[q*12+v*3+axis]==bright.positions[(q-4)*12+v*3+axis]);
        }
    }
    Q::cursor_us=2000000;assert(Q::CursorGlow()==0);
    for(bool leaving:{false,true}) {
        Q::leaving=leaving;
        Q::motion_us=0;assert(Q::RandomizerAlpha()==(leaving?1:0));
        Q::motion_us=75000;assert(Q::RandomizerAlpha()==.5f);
        Q::motion_us=150000;assert(Q::RandomizerAlpha()==(leaving?0:1));
    }
    Q::leaving=false;
    edited.quest_stones={{{-400,0,4},{400,0,4}}};
    for(float offset:{480.f,-480.f}) {
        const auto off=OOT3D::FileSelect::QuestStones(1,{offset,offset},edited);
        for(unsigned q:{1u,3u}) {
            const auto r=OOT3D::FileSelect::Bounds(off,q,q);
            assert(offset>0?r.left>400:r.right<0);
        }
    }
    m.Allocate(Q::System,0x24);m.Allocate(Q::LiveSave,Q::SaveSize);
    m.Allocate(Q::Exists,24);m.Allocate(Q::Saves,6*Q::Stride);
    m.words[Q::Owner+0x10]=3;m.words[Q::Owner+0x14]=0;
    m.words[0x50af14]=m.words[0x50af18]=m.words[0x50af30]=m.words[0x50af2c]=0;
    std::array<U32,16> r{};r[13]=0x8000000;r[14]=0x123456;
    m.Allocate(r[13],8); // Native sound call's volume and reverb pointer arguments.
    Q::Hook hook;
    // Title bypass works with BOTH locked and unlocked system data. Other
    // unlock-test callers still receive the true original byte.
    for(unsigned unlocked:{0u,0xbeu}) {
        Q::SetByte(m,Q::System,unlocked);
        for(U32 lr:{0x348104u,0x18cd34u,0x1daaf4u}) {
            r[0]=Q::System;r[14]=lr;hook.Handle(m,r,0);assert(r[0]==0);
        }
        r[0]=Q::System;r[14]=0x123456;hook.Handle(m,r,0);assert(r[0]==unlocked);
    }
    Q::SetMode(m,1);r[0]=Q::System;r[14]=0x123456;hook.Handle(m,r,1);
    assert(r[0]==0 && r[15]==0x123456 && Q::Byte(m,Q::System+31)==0xef);
    std::int64_t now=1000000;
    const auto enter=[&] {
        hook.Handle(m,r,2,now);assert(Q::choosing && hook.modal && !Q::ready);
        assert(hook.sound_only==Q::Hook::Sound::Select);
        hook.Handle(m,r,4,now);assert(r[15]==0x4336f8 && hook.sound_only==Q::Hook::Sound::None);
        m.words[0x50af18]=1;hook.Dispatch(m,r,now+16667);assert(hook.modal && !hook.confirmed);
        assert(Q::departing_files && Q::ReturnOffset(0,2)<0 && Q::ReturnOffset(1,2)==0);
        m.words[0x50af18]=0;now+=Q::SlideUs+2*Q::StaggerUs;
        hook.Dispatch(m,r,now);assert(!Q::departing_files && !Q::ready && Q::motion_us==0);
        now+=Q::SlideUs+Q::StaggerUs;
        hook.Dispatch(m,r,now);assert(Q::ready);
    };
    // Cancel creates no save, plays the native Back cue, and does not leak B.
    const auto save_before=m.words;
    enter();
    unsigned toggles=0;ToggleFiles toggle_files{{},&toggles};
    m.words[0x50af18]=0x200;hook.Dispatch(m,r,now,toggle_files);
    assert(toggles==1 && Q::hop_us==0 && !hook.confirmed && Q::RandomizerHop()==0);
    assert(r[15]==0x4301c4 && r[0]==0x01000129 && r[1]==0 && r[2]==4);
    assert(r[3]==m.Read(0x43045c) && m.Read(*m.Read(r[13]))==0x3f400000);
    assert(m.Read(r[13]+4)==m.Read(0x430458));
    hook.Handle(m,r,5,now);
    m.words[0x50af18]=0x300; // Both shoulders held still count as one toggle.
    hook.Dispatch(m,r,now+Q::HopUpUs,toggle_files);
    assert(toggles==1 && Q::RandomizerHop()==-3);
    hook.Dispatch(m,r,now+Q::HopUpUs+Q::HopDownUs/2,toggle_files);
    assert(toggles==1 && std::abs(Q::RandomizerHop()+1.5f)<.001f);
    hook.Dispatch(m,r,now+Q::HopUpUs+Q::HopDownUs,toggle_files);
    assert(toggles==1 && Q::RandomizerHop()==0);
    m.words[0x50af18]=0;hook.Dispatch(m,r,now+200000,toggle_files);
    m.words[0x50af18]=0x100;hook.Dispatch(m,r,now+216667,toggle_files);assert(toggles==2);
    assert(r[0]==Q::RandomizerToggleSound && r[15]==0x4301c4);
    hook.Handle(m,r,5,now+216667);
    // Exercise the real HID shoulder filter before the menu, including ZL/ZR
    // that are absent from the native pad word with swap off.
    for(bool swapped:{false,true}) {
        OOT3D::ControlSwap::shoulders=swapped;
        OOT3D::ControlSwap::Shoulders mapping;
        unsigned buttons=0,triggers=0;mapping.Filter(buttons,triggers,true);
        for(unsigned physical=0;physical<4;++physical) {
            buttons=triggers=0;mapping.Filter(buttons,triggers,true);
            m.words[0x50af18]=0;hook.Dispatch(m,r,now,toggle_files);
            buttons=physical==0?0x200:physical==1?0x100:0;
            triggers=physical==2?1:physical==3?2:0;
            mapping.Filter(buttons,triggers,true);
            m.words[0x50af18]=buttons;
            const auto before=toggles;hook.Dispatch(m,r,now,toggle_files);
            assert(toggles==before+1);hook.Handle(m,r,5,now);
            hook.Dispatch(m,r,now+16667,toggle_files);assert(toggles==before+1);
            // A delayed native shoulder repeat after physical release must
            // not cause a second toggle.
            buttons=triggers=0;mapping.Filter(buttons,triggers,true);
            m.words[0x50af18]=0x300;hook.Dispatch(m,r,now+33334,toggle_files);
            assert(toggles==before+1);
        }
    }
    OOT3D::ControlSwap::shoulders=false;
    OOT3D::ControlSwap::file_menu_shoulders=-1;
    const auto toggles_before_generation=toggles;
    // Generation owns the draft until it completes/cancels. R cannot change it.
    hook.generating=true;Q::ready=false;m.words[0x50af18]=0;
    struct BusyFiles : ToggleFiles {
        int Prepare(Memory&,unsigned,bool) {return 0;}
    } busy_files{{{},&toggles}};
    hook.Dispatch(m,r,now+233334,busy_files);
    m.words[0x50af18]=0x100;hook.Dispatch(m,r,now+250000,busy_files);assert(toggles==toggles_before_generation);
    hook.generating=false;
    now+=300000;m.words[0x50af18]=2;hook.Dispatch(m,r,now);
    assert(Q::choosing && Q::cancelling && !Q::ready && r[15]==0x430138);
    m.words[0x50af18]=0x100;hook.Dispatch(m,r,now,toggle_files);assert(toggles==toggles_before_generation);
    hook.Handle(m,r,6,now);assert(r[15]==0x4336f8);
    assert(hook.Dispatch(m,r,now));assert((Q::Offsets(2)==std::array{0.f,0.f}));
    hook.Dispatch(m,r,now+100000);assert(Q::Offsets(2)[0]>0 && Q::Offsets(2)[0]==Q::Offsets(2)[1]);
    assert(Q::ReturnOffset(0,2)>-480 && Q::ReturnOffset(1,2)==-480);
    // B sends both mode stones right on the same frame, with no MQ delay.
    for(std::int64_t t=16667;t<Q::SlideUs;t+=16667) {
        hook.Dispatch(m,r,now+t);
        const auto x=Q::Offsets(2);assert(x[0]>0 && x[0]==x[1]);
    }
    hook.Dispatch(m,r,now+Q::SlideUs);
    assert((Q::Offsets(2)==std::array{480.f,480.f}));
    now+=Q::SlideUs+Q::StaggerUs;assert(hook.Dispatch(m,r,now));
    assert(Q::choosing && Q::returning && Q::cancelling && hook.modal);
    assert(Q::ReturnOffset(0,2)==0 && Q::ReturnOffset(2,2)<0);
    now+=Q::StaggerUs;
    assert(hook.Dispatch(m,r,now) && !Q::returning && !Q::choosing && hook.block_cancel);
    m.words[0x50af18]=0;assert(!hook.Dispatch(m,r,now));
    for(U32 i=0;i<Q::SaveSize;i+=4)assert(m.Read(Q::LiveSave+i)==save_before.at(Q::LiveSave+i));
    for(unsigned slot=0;slot<3;++slot)assert(!Q::Occupied(m,slot));
    // MQ navigation uses the exact file-cursor sound once per changed selection.
    enter();m.words[0x50af30]=0x10;hook.Dispatch(m,r,now);
    assert(Q::highlighted==1 && r[15]==0x4301a8 && hook.sound_only==Q::Hook::Sound::Move);
    hook.Handle(m,r,5,now);assert(r[15]==0x4336f8 && m.words[Q::Owner+0x14]==0);
    hook.Dispatch(m,r,now);assert(hook.sound_only==Q::Hook::Sound::None);
    m.words[0x50af30]=0;m.words[0x50af18]=1;hook.Dispatch(m,r,now);
    assert(hook.confirmed && Q::choosing && Q::leaving && !Q::ready && Q::Byte(m,Q::System+31)==0xef);
    assert(r[15]==Q::Sites[2].pc+4 && hook.sound_only==Q::Hook::Sound::Select);
    hook.Handle(m,r,4,now);assert(r[15]==0x4336f8);
    const U32 sp=r[13];hook.Dispatch(m,r,now+100000);assert(r[15]==0x4336f8);
    now+=Q::SlideUs+Q::StaggerUs;hook.Dispatch(m,r,now);
    assert(r[15]==Q::Sites[4].pc && r[13]==sp && hook.awaiting_name && !hook.modal);
    hook.Handle(m,r,4,now);assert(r[5]==m.Read(0x430464) && hook.sound_only==Q::Hook::Sound::None);
    m.words[Q::Owner+0x10]=5;m.words[Q::Owner+0x28]=0;m.words[Q::Owner+0x54]=0;
    assert(!hook.Dispatch(m,r,now) && m.words[Q::Owner+0x28]==5 && Q::choosing);
    m.words[Q::Owner+0x10]=0;hook.Dispatch(m,r,now);assert(!Q::choosing);
    m.words[Q::Owner+0x10]=3;
    // Entrance/exit use the same easing power as file cards, independent of FPS.
    for(bool exit:{false,true})for(float strength:{0.f,2.f,4.f}) {
        Q::leaving=exit;
        std::array<float,2> last=exit?std::array{0.f,0.f}:std::array{480.f,480.f};
        for(std::int64_t t=0;t<Q::SlideUs+Q::StaggerUs;t+=16667) {
            Q::motion_us=t;const auto x=Q::Offsets(strength);
            for(unsigned i=0;i<2;++i) {assert(x[i]<=last[i]);last[i]=x[i];}
            const auto cards=OOT3D::FileSelect::QuestStones(1,x);
            const auto shadows=OOT3D::FileSelect::QuestShadows(cards,.55f);
            for(unsigned i=0;i<2;++i) {
                assert(std::abs(cards.positions[(i*2+1)*12]-(77.525f+154.f*i+x[i]))<.001f);
                assert(std::abs(shadows.positions[(i*9+8)*12]+40-cards.positions[(i*2+1)*12])<.001f);
            }
        }
        Q::motion_us=Q::SlideUs+Q::StaggerUs;
        assert(Q::Offsets(strength)==(exit?std::array{-480.f,-480.f}:std::array{0.f,0.f}));
    }
    Q::leaving=false;
    // The original mode flag is inside the native CRC and copied with the actual save bytes.
    for(unsigned slot=0;slot<3;++slot) {
        const unsigned mode=slot%2;Q::SetMode(m,mode);
        m.words[Q::LiveSave+0x14d8]=0;
        assert(Q::Byte(m,Q::LiveSave+Q::ModeOffset)==mode);
        const auto crc=CRC(m,Q::LiveSave);
        m.words[Q::LiveSave+0x14d8]|=crc;
        for(U32 i=0;i<Q::SaveSize;i+=4)m.words[Q::Saves+slot*Q::Stride+i]=*m.Read(Q::LiveSave+i);
        m.words[Q::Exists+slot*4]=1;assert(Q::Mode(m,slot)==mode);
        const U32 end=Q::Saves+slot*Q::Stride+0x14d8;
        m.words[end]&=0xffff0000;assert(CRC(m,Q::Saves+slot*Q::Stride)==crc);
        Q::SetByte(m,Q::Saves+slot*Q::Stride+Q::ModeOffset,1-mode);
        assert(CRC(m,Q::Saves+slot*Q::Stride)!=crc);
        Q::SetByte(m,Q::Saves+slot*Q::Stride+Q::ModeOffset,mode);
        m.words[end]|=crc;
    }
    // Fresh Hook simulates a restart: mode is recovered solely from the file.
    Q::Hook restarted;
    for(unsigned slot=0;slot<3;++slot) {
        Q::SetMode(m,1-slot%2);r[1]=slot;restarted.Handle(m,r,3);
        assert(Q::Byte(m,Q::System+31)==(slot%2?0xefu:0u));
        assert(r[15]==0x44735c);
        m.words[Q::Owner+0x14]=slot;restarted.Handle(m,r,2);assert(!Q::choosing);
    }
    // Badges use the per-file mode, never the live gameplay mode. Their
    // geometry inherits card scaling, motion and alpha, with the D-pad crop.
    OOT3D::FileSelect::Geometry atlas;atlas.count=184;atlas.colors.fill(1);
    for(unsigned slot=0;slot<3;++slot)for(unsigned q=6+37*slot;q<9+37*slot;++q) {
        const float x=16.f+40*slot,y=48.f+52*slot;
        const std::array<float,12> v{x,y,0,x+288,y,0,x,y+44,0,x+288,y+44,0};
        std::copy(v.begin(),v.end(),atlas.positions.begin()+q*12);
        for(unsigned c=3;c<16;c+=4)atlas.colors[q*16+c]=.4f;
    }
    Q::SetMode(m,0);auto badge=OOT3D::FileSelect::QuestBadges(m,atlas);assert(badge.count==1);
    assert(badge.positions[0]==64 && badge.positions[1]==106 && badge.positions[3]-badge.positions[0]==16);
    assert(badge.colors[3]==.4f && badge.uvs[0]==192.5f/256 && badge.uvs[1]==86.5f/128);
    for(unsigned q=43;q<46;++q)for(unsigned v=0;v<4;++v) {
        atlas.positions[q*12+v*3]=atlas.positions[q*12+v*3]*.5f+10;
        atlas.positions[q*12+v*3+1]*=.5f;
    }
    badge=OOT3D::FileSelect::QuestBadges(m,atlas);assert(badge.count==1);
    assert(badge.positions[0]==42 && badge.positions[1]==53 && badge.positions[3]-badge.positions[0]==8);
    // Native erase clears save flags before replacing the displayed name and
    // hearts. The badge must survive that gap, then leave with the old card.
    for(unsigned slot=0;slot<3;++slot) {
        Memory deleting;OOT3D::FileSelect::RandomizerBadgeState badges;
        deleting.words[Q::Owner+0x10]=37;deleting.words[Q::Owner+0x14]=slot;
        deleting.words[Q::Owner+0x2c]=0;deleting.words[Q::Exists+4*slot]=1;
        deleting.words[0x55b53c+4*slot]=1;
        deleting.words[Q::Saves+slot*Q::Stride+0x2c]=0xa7000000;
        auto card=atlas;const unsigned heart=11+37*slot;
        const std::array<float,12> heart_positions{200,60,0,210,60,0,200,70,0,210,70,0};
        std::copy(heart_positions.begin(),heart_positions.end(),card.positions.begin()+heart*12);
        const auto bounds=OOT3D::FileSelect::PartBounds(card);
        const auto draw=[&] {
            Status::Geometry label;
            OOT3D::FileSelect::RandomizerBadges(deleting,label,*font,card,{},bounds,badges.visible);
            return label;
        };
        badges.Frame(deleting);const auto before=draw();assert(before.count==45);
        for(unsigned state:{39u,42u,44u}) {
            deleting.words[Q::Owner+0x10]=state;badges.Frame(deleting);
            assert(draw().count==before.count);
        }
        deleting.words[Q::Exists+4*slot]=0;
        deleting.words[Q::Saves+slot*Q::Stride+0x2c]=0;
        const auto erased=deleting.words;
        for(unsigned frame=0;frame<30;++frame) {
            badges.Frame(deleting);const auto during=draw();
            assert(during.count==before.count && during.positions==before.positions && during.colors==before.colors);
            assert(deleting.words==erased);
        }
        deleting.words[0x55b53c+4*slot]=0;badges.Frame(deleting);
        assert(!draw().count && badges.visible==0);
        // Reusing that slot for an ordinary file must never inherit RANDO.
        deleting.words[Q::Owner+0x10]=3;deleting.words[Q::Exists+4*slot]=1;
        deleting.words[0x55b53c+4*slot]=1;badges.Frame(deleting);
        assert(!draw().count);
    }
    std::cout<<"RANDO erase presentation: all three slots retain pose/alpha until New Game, without changing saves or leaking to reused slots\n";
    // Native copying preserves the mode, including an overwrite of Normal by MQ.
    for(U32 i=0;i<Q::SaveSize;i+=4)m.words[Q::Saves+2*Q::Stride+i]=*m.Read(Q::Saves+Q::Stride+i);
    assert(Q::Mode(m,2)==1);
    m.words[Q::Exists+8]=0;assert(!Q::Occupied(m,2));
    m.words[Q::Owner+0x14]=2;restarted.Handle(m,r,2);assert(Q::choosing);
    restarted.Handle(m,r,4); // Initial native select sound returned.
    m.words[0x50af18]=0;restarted.Dispatch(m,r,Q::SlideUs+2*Q::StaggerUs);
    const auto ready_at=2*Q::SlideUs+3*Q::StaggerUs;
    restarted.Dispatch(m,r,ready_at);
    Q::pointer_action=1;restarted.Dispatch(m,r,ready_at);assert(restarted.confirmed);
    assert(Q::Byte(m,Q::System+31)==0); // Reusing an erased slot may change its mode.
    for(unsigned slot=3;slot<6;++slot)assert(m.Read(Q::Exists+slot*4)==0);
    // The displayed seed belongs to this occupied, randomized action menu.
    OOT3D::Randomizer::FileStatus::Publish(0,"saved-seed");
    m.words[Q::Owner+0x14]=0;m.words[Q::Exists]=1;
    Q::SetByte(m,Q::Saves+0x2f,0xa7);
    for(unsigned state=0;state<45;++state) {
        m.words[Q::Owner+0x10]=state;
        assert(OOT3D::FileSelect::SelectedSeed(m)==
            ((state==4 || state==7 || state==8 || state==10)?"saved-seed":""));
    }
    m.words[Q::Owner+0x10]=8;m.words[Q::Owner+0x14]=1;
    assert(OOT3D::FileSelect::SelectedSeed(m).empty());
    m.words[Q::Owner+0x14]=0;Q::SetByte(m,Q::Saves+0x2f,0);
    assert(OOT3D::FileSelect::SelectedSeed(m).empty());
    Q::SetByte(m,Q::Saves+0x2f,0xa7);m.words[Q::Exists]=0;
    assert(OOT3D::FileSelect::SelectedSeed(m).empty());
    OOT3D::Randomizer::FileStatus::Publish();
    // Native loading state: departure and cancellation animate while the
    // backend is pending; name entry must wait for both completion and fade.
    struct GenerationFiles : Q::OrdinaryFiles {
        int* result;unsigned* cancels;
        int Prepare(Memory&,unsigned,bool) {return *result;}
        void Cancel() {++*cancels;}
    };
    int result=0;unsigned cancelled=0;GenerationFiles files{{},&result,&cancelled};
    Q::Hook loading_hook;
    loading_hook.modal=true;loading_hook.slot=0;Q::choosing=true;
    m.words[Q::Owner+0x10]=3;m.words[Q::Exists]=0;
    now=50000000;loading_hook.started_us=now-Q::SlideUs-Q::StaggerUs;
    m.words[0x50af18]=1;m.words[0x50af14]=0;
    loading_hook.Dispatch(m,r,now,files);
    assert(loading_hook.generating && Q::loading && Q::loading_label && Q::leaving && !Q::ready);
    assert(r[15]==Q::Sites[2].pc+4 && loading_hook.sound_only==Q::Hook::Sound::Select);
    loading_hook.Handle(m,r,4,now,files);
    assert(r[15]==0x4336f8 && loading_hook.sound_only==Q::Hook::Sound::None);
    m.words[0x50af18]=0;loading_hook.Dispatch(m,r,now+200000,files);
    assert(Q::loading_alpha==1 && Q::Offsets(2)[0]<0 && Q::Offsets(2)[1]<0);
    assert(r[15]==0x4336f8 && loading_hook.sound_only==Q::Hook::Sound::None);
    m.words[0x50af18]=2;loading_hook.Dispatch(m,r,now+220000,files);
    assert(cancelled==1 && !loading_hook.generating && !Q::loading && !Q::loading_label);
    assert(Q::generation_return && !Q::leaving && Q::Offsets(2)[0]==-480);
    loading_hook.Handle(m,r,6,now+220000,files);
    m.words[0x50af18]=0;loading_hook.Dispatch(m,r,now+750000,files);
    assert(Q::ready && Q::loading_alpha==0 && Q::Offsets(2)[0]==0);
    now+=800000;m.words[0x50af18]=1;loading_hook.Dispatch(m,r,now,files);
    assert(loading_hook.generating);
    m.words[0x50af18]=0;loading_hook.Dispatch(m,r,now+500000,files);
    result=1;loading_hook.Dispatch(m,r,now+516667,files);
    assert(loading_hook.confirmed && !Q::loading && Q::loading_alpha==1 && Q::loading_label);
    assert(Q::Offsets(2)[0]==-480 && Q::Offsets(2)[1]==-480);
    assert(r[15]==Q::Sites[2].pc+4); // Normal completion/select audio.
    loading_hook.Handle(m,r,4,now+516667,files);
    loading_hook.Dispatch(m,r,now+550000,files);assert(!loading_hook.awaiting_name);
    loading_hook.Dispatch(m,r,now+716667,files);
    assert(loading_hook.awaiting_name && !loading_hook.modal && !Q::loading_label);
    assert(r[15]==Q::Sites[4].pc && Q::loading_alpha==0);
    // The Triforce uses the real texture, same saved HUD pose, and smooth
    // staggered opacity for its three native triangle layers.
    const auto throbber=OOT3D::LoadingIcon::MakePixels(Read(std::string(argv[2])+"/misc/throbber.ctxb"));
    assert(throbber);
    const OOT3D::LoadingIcon::Anchor anchor{380,220,8};
    auto icon=OOT3D::LoadingIcon::Draw(0,1,{12,-9,2},anchor);
    assert(icon.count==3);
    const auto box=OOT3D::FileSelect::Bounds(icon,0,2);
    assert(box.X()==392 && box.Y()==211 && box.right-box.left==32);
    const auto invisible=OOT3D::LoadingIcon::Draw(0,0,{},anchor);
    for(unsigned i=3;i<48;i+=4)assert(invisible.colors[i]==0);
    assert(icon.colors!=OOT3D::LoadingIcon::Draw(8333,1,{12,-9,2},anchor).colors);
    // The file-menu caller caps this instance, leaving the title icon alone.
    std::atomic_store(&OOT3D::LoadingIcon::pixels,std::make_shared<const OOT3D::LoadingIcon::Pixels>(*throbber));
    OOT3D::FileSelect::Hook loading_draw;
    loading_draw.source=0x8000000;m.words[loading_draw.source]=0x4ebd60;
    m.words[loading_draw.source+4]=0x8010000;m.words[0x8010000]=0x8020000;
    m.words[loading_draw.source+0x1dc]=0x8030000;
    Q::cursor_us=0;Q::loading_alpha=1;
    loading_draw.BeginLoading(m,r);
    assert(loading_draw.loading_icon.Pending());
    float peak=0;
    for(unsigned i=3;i<48;i+=4)peak=std::max(peak,loading_draw.loading_icon.geometry.colors[i]);
    assert(std::abs(peak-.8f)<.0001f);
    // Reject the same MQ/logic combinations as the generator, before any
    // preparation, departure, loading UI or save mutation can begin.
    namespace R=OOT3D::Randomizer;
    for(bool enabled:{false,true})for(bool mq:{false,true})
    for(int logic=0;logic<4;++logic)for(int enemy=0;enemy<2;++enemy)for(int souls=0;souls<3;++souls) {
        R::Config config;config.enabled=enabled;
        config.advanced={{"Logic",logic},{"Enemizer",enemy},{"ShuffleEnemySouls",souls}};
        assert(R::UnsupportedMasterQuest(config,mq)==(enabled && mq && logic<2 && (enemy || souls==1)));
    }
    struct RejectedFiles : Q::OrdinaryFiles {
        unsigned* prepares;
        bool RejectChoice(bool mq) {return mq;}
        int Prepare(Memory&,unsigned,bool) {++*prepares;return 1;}
    };
    unsigned prepares=0;RejectedFiles rejected{{},&prepares};Q::Hook rejection;
    rejection.modal=true;rejection.slot=0;rejection.choice=1;Q::choosing=true;
    m.words[Q::Owner+0x10]=3;m.words[Q::Exists]=0;
    m.words[0x50af18]=1;m.words[0x50af14]=m.words[0x50af30]=m.words[0x50af2c]=0;
    now=90000000;rejection.started_us=now-Q::SlideUs-Q::StaggerUs;
    const auto archive_before=Read(std::string(argv[2])+"/message/us/us.qm");
    auto archive=archive_before;assert(OOT3D::LoadingPrompt::Prepare(archive));
    const auto messages=std::atomic_load(&OOT3D::LoadingPrompt::entry);
    assert(std::equal(archive_before.begin(),archive_before.end(),archive.begin()));
    namespace GP=OOT3D::Randomizer::GenerationProgress;
    {
        GP::Presentation p;
        p.Frame(100,1);p.Frame(100+GP::Presentation::HoldUs-1,4);
        assert(p.displayed==1 && p.opacity==1);
        auto tick=100+GP::Presentation::HoldUs;
        p.Frame(tick,4);assert(p.displayed==1 && p.opacity==1);
        p.Frame(tick+75000,5);assert(p.displayed==1 && std::abs(p.opacity-.5f)<.0001f);
        p.Frame(tick+150000,6);assert(p.displayed==6 && p.opacity==0);
        p.Frame(tick+225000,7);assert(p.displayed==6 && std::abs(p.opacity-.5f)<.0001f);
        p.Frame(tick+300000,7);assert(p.displayed==6 && p.opacity==1);
        p.Frame(tick+375000,7);assert(p.displayed==6 && std::abs(p.opacity-.5f)<.0001f);
        p.Frame(tick+450000,7);assert(p.displayed==7 && p.opacity==0);
        p.Frame(tick+600000,7);assert(p.displayed==7 && p.opacity==1);
        p.Frame(0,8);assert(p.displayed==1 && p.opacity==1); // Rewound clock.
    }
    auto progress=GP::Begin();assert(GP::Attempt()==1);
    GP::UpdatePresentation(0,true);
    assert(OOT3D::LoadingPrompt::GenerationMessageId()==OOT3D::LoadingPrompt::MessageId);
    std::int64_t presentation_time=GP::Presentation::HoldUs;
    for(unsigned attempt=2;attempt<=GP::AttemptLimit;++attempt) {
        progress->Observe(std::to_string(attempt)+"\n");assert(GP::Attempt()==attempt);
        GP::UpdatePresentation(presentation_time,true);
        GP::UpdatePresentation(presentation_time+GP::Presentation::FadeUs,true);
        assert(GP::TransitionOpacity()==0);
        assert(OOT3D::LoadingPrompt::PresentationOpacity(presentation_time,false,true)==0);
        GP::UpdatePresentation(presentation_time+2*GP::Presentation::FadeUs,true);
        presentation_time+=3*GP::Presentation::FadeUs;
        assert(GP::TransitionOpacity()==1);
        const auto id=OOT3D::LoadingPrompt::GenerationMessageId();
        assert(OOT3D::LoadingPrompt::RetryMessage(id));
        const auto offset=messages->retry_offsets[attempt-2];
        assert(OOT3D::Messages::Read32(archive,offset)==id);
        for(unsigned language:{1u,4u,5u,6u,7u}) {
            const auto label=GP::Label(attempt,language);
            const auto at=OOT3D::Messages::Read32(archive,offset+16+language*8);
            const auto length=OOT3D::Messages::Read32(archive,offset+20+language*8);
            assert(length==label.size()+8 && archive[at]==0x7f && archive[at+1]==4);
            assert(std::string(archive.begin()+at+4,archive.begin()+at+4+label.size())==label);
            assert(label.size()<=19); // French: "Tentative 10 sur 10".
            assert(Status::Measure(*font,label,1.f)<330);
        }
        const auto choosing=Q::choosing.load(),loading=Q::loading_label.load(),cancelling=Q::cancelling.load();
        Q::choosing=true;Q::loading_label=true;Q::cancelling=false;
        assert(Q::PromptMessage()==id);
        Q::choosing=choosing;Q::loading_label=loading;Q::cancelling=cancelling;
        assert(std::abs(OOT3D::LoadingPrompt::PresentationOpacity(1000000)-.45f)<.0001f);
    }
    for(const auto bad:{"0","11","-1","","junk","9x","2\n"})progress->Observe(bad);
    assert(GP::Attempt()==10);
    GP::Reset();assert(GP::Attempt()==1);
    auto next=GP::Begin();progress->Observe("10\n");assert(GP::Attempt()==1);
    GP::UpdatePresentation(0,true);assert(GP::DisplayedAttempt()==1 && GP::TransitionOpacity()==1);
    next->Observe("2\n");assert(GP::Attempt()==2);
    GP::UpdatePresentation(GP::Presentation::HoldUs-1,true);assert(GP::DisplayedAttempt()==1);
    GP::UpdatePresentation(GP::Presentation::HoldUs,true);
    GP::UpdatePresentation(GP::Presentation::HoldUs+GP::Presentation::FadeUs,true);
    {
        const auto choosing=Q::choosing.load(),loading=Q::loading_label.load(),cancelling=Q::cancelling.load();
        const auto controller=Q::prompt_controller.load(),message=Q::prompt_message.load();
        Q::choosing=true;Q::loading_label=true;Q::cancelling=false;
        auto regs=r;
        label_hook.Handle(m,regs,7);const auto built=regs[1];
        next->Observe("3\n");
        GP::UpdatePresentation(GP::Presentation::HoldUs+2*GP::Presentation::FadeUs,true);
        GP::UpdatePresentation(GP::Presentation::HoldUs+3*GP::Presentation::FadeUs,true);
        m.words[0x9900018]=0;
        regs[0]=0x8000000;regs[1]=0x9900000;label_hook.Handle(m,regs,8);
        assert(Q::prompt_message==built && Q::prompt_message!=Q::PromptMessage());
        Q::choosing=choosing;Q::loading_label=loading;Q::cancelling=cancelling;
        Q::prompt_controller=controller;Q::prompt_message=message;
    }
    GP::Reset();
    GP::UpdatePresentation(presentation_time,false);
    assert(GP::DisplayedAttempt()==1 && GP::TransitionOpacity()==1);

    for(unsigned language:{1u,5u,7u}) {
        const auto at=OOT3D::Messages::Read32(archive,messages->error_offset+16+language*8);
        assert(archive[at+4]==0x7f && archive[at+5]==0x1d && archive[at+6]==0x41);
        const auto disconnect=messages->room_offsets[static_cast<unsigned>(OOT3D::RoomJoin::Status::Disconnecting)];
        const auto red=OOT3D::Messages::Read32(archive,disconnect+16+language*8);
        assert(archive[red+4]==0x7f && archive[red+5]==0x1d && archive[red+6]==0x41);
    }
    {
        constexpr U32 table=0x9800000,owner=0x9900000;
        m.words[owner+4]=table;m.words[table]=0x4d51;
        m.words[table+messages->signature]=0x4c544f4f;
        m.words[table+messages->signature+4]=0x504d4f52;
        auto lookup=r;lookup[14]=0x123400;
        for(const auto id:{OOT3D::LoadingPrompt::MessageId,OOT3D::LoadingPrompt::ErrorMessageId,OOT3D::LoadingPrompt::WaitingMessageId}) {
            lookup[0]=owner;lookup[1]=id;
            assert(OOT3D::LoadingPrompt::Select(m,lookup));
            assert(lookup[0]==table+(id==OOT3D::LoadingPrompt::MessageId?messages->offset:
                id==OOT3D::LoadingPrompt::WaitingMessageId?messages->waiting_offset:messages->error_offset));
        assert(lookup[15]==lookup[14]);
        }
        for(unsigned status=0;status<OOT3D::LoadingPrompt::RoomStatusCount;++status) {
            lookup[0]=owner;lookup[1]=OOT3D::LoadingPrompt::WaitingMessageId-status;
            assert(OOT3D::LoadingPrompt::Select(m,lookup));
            assert(lookup[0]==table+messages->room_offsets[status]);
        }
        for(unsigned i=0;i<OOT3D::LoadingPrompt::RetryCount;++i) {
            lookup[0]=owner;lookup[1]=OOT3D::LoadingPrompt::RetryMessageId-i;
            assert(OOT3D::LoadingPrompt::Select(m,lookup));
            assert(lookup[0]==table+messages->retry_offsets[i]);
        }
        lookup[0]=owner;lookup[1]=0x900;
        assert(!OOT3D::LoadingPrompt::Select(m,lookup) && lookup[0]==owner);
    }
    rejection.Dispatch(m,r,now,rejected);
    assert(Q::mq_error && Q::ready && !Q::leaving && !Q::loading && !rejection.confirmed && prepares==0);
    assert(Q::PromptMessage()==OOT3D::LoadingPrompt::ErrorMessageId);
    assert(r[0]==Q::RejectionSound && r[15]==0x4301c4 && m.Read(0x43d448)==Q::RejectionSound);
    rejection.Handle(m,r,5,now,rejected);assert(r[15]==0x4336f8);
    rejection.Dispatch(m,r,now+16667,rejected);
    assert(prepares==0 && Q::Offsets(2)[0]==0 && std::abs(Q::Offsets(2)[1])>0);
    m.words[0x50af18]=0;rejection.Dispatch(m,r,now+Q::ShakeUs,rejected);
    assert(Q::Offsets(2)[0]==0 && Q::Offsets(2)[1]==0);
    m.words[0x50af18]=0x20;rejection.Dispatch(m,r,now+Q::ShakeUs+16667,rejected);
    assert(!Q::mq_error && Q::PromptMessage()==0x900 && rejection.choice==0);
    rejection.Handle(m,r,5,now,rejected);
    m.words[0x50af18]=1;rejection.Dispatch(m,r,now+Q::ShakeUs+33334,rejected);
    assert(prepares==1 && rejection.confirmed && Q::leaving);
    {
        namespace E=R::FileStatus;
        namespace L=OOT3D::LoadingPrompt;
        assert(E::GeneratorError("No beatable placement found after five attempts.")==E::Error::Placement);
        assert(E::GeneratorError("No beatable placement found after ten attempts.")==E::Error::Placement);
        assert(E::GeneratorError("Conflicting goals and item pool")==E::Error::Settings);
        assert(E::GeneratorError("unrecognized helper failure")==E::Error::Generation);
        // Native archive entries exist in every supported language, are instant
        // red, restore white, and preserve every original byte/table index.
        for(unsigned i=3;i<static_cast<unsigned>(E::Error::Count);++i) {
            const auto error=static_cast<E::Error>(i);
            const auto id=L::ErrorId(error);
            assert(L::FileErrorMessage(id) && L::NoticeMessage(id));
            constexpr U32 table=0x9800000,owner=0x9900000;
            auto lookup=r;lookup[0]=owner;lookup[1]=id;lookup[14]=0x123400;
            assert(L::Select(m,lookup));
            const auto offset=messages->notice_offsets[i-3];
            assert(lookup[0]==table+offset && lookup[15]==lookup[14]);
            for(unsigned language:{1u,4u,5u,6u,7u}) {
                const auto label=E::ErrorLabel(error,language);
                assert(!label.empty());
                const auto at=OOT3D::Messages::Read32(archive,offset+16+language*8);
                const auto size=OOT3D::Messages::Read32(archive,offset+20+language*8);
                assert(archive[at]==0x7f && archive[at+1]==4 && archive[at+6]==0x41);
                assert(std::string(archive.begin()+at+7,archive.begin()+at+size-7)==label);
                assert(archive[at+size-5]==0); // Restore white before the terminator.
                assert(Status::Measure(*font,label,1.f)<330);
                for(auto cp:Status::Codepoints(label))assert(cp==' ' || (font->Find(cp) && font->Find(cp)->code==cp));
            }
            assert(E::ErrorLabel(error,4)==E::ErrorLabel(error,5));
            assert(E::ErrorLabel(error,6)==E::ErrorLabel(error,7));
        }
        // Multiplayer startup also uses native red text, with the correct
        // language and a one-shot loading-icon shake, rather than an F1 overlay.
        const auto previous_font=Status::GetAssets();
        const auto previous_source=m.words[OOT3D::HUD::Action+0x20];
        std::atomic_store(&Status::assets,std::make_shared<const Status::Assets>(*font));
        for(unsigned language:{1u,2u,5u}) {
            OOT3D::UILanguage::active=language;
            OOT3D::MultiplayerStartup::phase=OOT3D::MultiplayerStartup::Phase::Failed;
            ++OOT3D::MultiplayerStartup::generation;
            OOT3D::MultiplayerLoading::Hook startup;
            m.words[OOT3D::HUD::Action+0x20]=loading_draw.source;
            startup.Begin(m,r,0x123400,now);
            assert(startup.error_geometry.count>0);
            for(unsigned i=0;i<startup.error_geometry.count*16;i+=4) {
                assert(startup.error_geometry.colors[i]==1 && startup.error_geometry.colors[i+1]==.18f);
            }
            for(unsigned i=0;i<startup.error_geometry.count*12;i+=3)
                assert(startup.error_geometry.positions[i]>=20 && startup.error_geometry.positions[i]<=381);
            const auto first=OOT3D::FileSelect::Bounds(startup.icon_geometry,0,2);
            startup.Begin(m,r,0x123400,now+16667);
            const auto shaken=OOT3D::FileSelect::Bounds(startup.icon_geometry,0,2);
            assert(std::abs(first.X()-shaken.X())>0);
            startup.Begin(m,r,0x123400,now+250000);
            assert(OOT3D::FileSelect::Bounds(startup.icon_geometry,0,2).X()==first.X());
            OOT3D::MultiplayerStartup::phase=OOT3D::MultiplayerStartup::Phase::Ready;
            startup.Begin(m,r,0x123400,now+266667);assert(!startup.error_geometry.count);
        }
        OOT3D::UILanguage::active=1;OOT3D::MultiplayerStartup::phase=OOT3D::MultiplayerStartup::Phase::Idle;
        std::atomic_store(&Status::assets,previous_font);
        m.words[OOT3D::HUD::Action+0x20]=previous_source;
        // Failure after background work: restore visible choices and start one
        // shake immediately; a held A must neither retry nor restart the shake.
        for(unsigned choice=0;choice<2;++choice) {
            Q::Hook failed;failed.modal=true;failed.slot=0;failed.choice=choice;Q::choosing=true;
            m.words[Q::Owner+0x10]=3;m.words[Q::Owner+0x14]=0;m.words[Q::Exists]=0;
            m.words[0x50af18]=1;m.words[0x50af14]=m.words[0x50af30]=m.words[0x50af2c]=0;
            now+=2000000;failed.started_us=now-Q::SlideUs-Q::StaggerUs;
            result=0;failed.Dispatch(m,r,now,files);failed.Handle(m,r,4,now,files);
            assert(failed.generating && Q::loading_label);
            E::Report(E::Error::Placement);result=-1;
            failed.Dispatch(m,r,now+500000,files);
            assert(!failed.generating && !Q::loading && !Q::loading_label && !Q::leaving && Q::ready);
            assert(Q::PromptMessage()==L::ErrorId(E::Error::Placement));
            assert(Q::notice_target==6+choice && Q::notice_shake_us==0 && Q::Offsets(2)[choice]==0);
            assert(r[0]==Q::RejectionSound && r[15]==0x4301c4);
            failed.Handle(m,r,5,now+500000,files);
            const auto cancel_count=cancelled;
            failed.Dispatch(m,r,now+516667,files);
            assert(cancelled==cancel_count && Q::notice_shake_us==16667);
            assert(std::abs(Q::Offsets(2)[choice])>0 && Q::Offsets(2)[1-choice]==0);
            m.words[0x50af18]=0;failed.Dispatch(m,r,now+750000,files);
            assert(Q::Offsets(2)[choice]==0 && Q::notice_error==E::Error::Placement);
            // Same error on a new attempt is a new rejection, not swallowed.
            m.words[0x50af18]=1;result=0;failed.Dispatch(m,r,now+800000,files);
            assert(Q::notice_error==E::Error::None && failed.generating);
            failed.Handle(m,r,4,now+800000,files);
            E::Report(E::Error::Placement);result=-1;
            failed.Dispatch(m,r,now+1300000,files);failed.Handle(m,r,5,now+1300000,files);
            assert(Q::notice_shake_us==0 && Q::notice_error==E::Error::Placement);
            m.words[0x50af18]=0;failed.Dispatch(m,r,now+1510000,files);
            m.words[0x50af18]=2;failed.Dispatch(m,r,now+1530000,files);
            assert(Q::notice_error==E::Error::None && Q::cancelling);
        }
        {
            struct ImmediateFailure : Q::OrdinaryFiles {
                int Prepare(Memory&,unsigned,bool) {E::Report(E::Error::Memory);return -1;}
            } immediate;
            Q::Hook failed;failed.modal=true;failed.slot=0;Q::choosing=true;
            m.words[Q::Owner+0x10]=3;m.words[Q::Owner+0x14]=0;m.words[Q::Exists]=0;
            m.words[0x50af18]=1;m.words[0x50af14]=m.words[0x50af30]=m.words[0x50af2c]=0;
            now+=2000000;failed.started_us=now-Q::SlideUs-Q::StaggerUs;
            failed.Dispatch(m,r,now,immediate);
            assert(Q::notice_error==E::Error::Memory && Q::notice_shake_us==0);
            assert(Q::PromptMessage()==L::ErrorId(E::Error::Memory));
            assert(!failed.generating && !Q::loading && !Q::loading_label && !Q::leaving);
            assert(r[0]==Q::RejectionSound && r[15]==0x4301c4);
            failed.Handle(m,r,5,now,immediate);
        }
        // File-management failures use the same prompt and the selected action,
        // and navigation dismisses them without reviving an old toast/event.
        Q::Hook managed;m.words[Q::Owner+0x10]=8;m.words[Q::Owner+0x14]=1;
        m.words[Q::Owner+0x18]=1;m.words[0x50af18]=m.words[0x50af14]=0;
        E::Report(E::Error::Storage);managed.Dispatch(m,r,now);
        assert(Q::notice_error==E::Error::Storage && Q::notice_target==4);
        managed.Dispatch(m,r,now+16667);assert(std::abs(Q::NoticeShake(4))>0 && Q::NoticeShake(3)==0);
        m.words[Q::Owner+0x18]=2;managed.Dispatch(m,r,now+33334);
        assert(Q::notice_error==E::Error::None);
        managed.Dispatch(m,r,now+50000);assert(Q::notice_error==E::Error::None);
        const auto cancelled_event=std::make_shared<std::atomic<bool>>(false);
        E::Report(E::Error::Placement,cancelled_event);*cancelled_event=true;
        managed.Dispatch(m,r,now+66667);assert(Q::notice_error==E::Error::None);
        E::Report(E::Error::Timeout,cancelled_event); // A worker finishing after B.
        managed.Dispatch(m,r,now+83334);assert(Q::notice_error==E::Error::None);
    }
    {
        struct InvalidFile : Q::OrdinaryFiles {
            Q::FileError error=Q::FileError::Incompatible;
            Q::FileError CheckStart(Memory&,unsigned) {return error;}
        } invalid_file;
        Q::Hook start_rejection;
        m.words[Q::Owner+0x10]=8;m.words[Q::Owner+0x14]=1;m.words[Q::Owner+0x18]=0;
        m.words[0x50af18]=1;m.words[0x50af14]=0;
        const auto original_save=m.Read(Q::Saves+Q::Stride);
        start_rejection.Handle(m,r,9,now,invalid_file);
        assert(m.Read(Q::Owner+0x10)==8 && m.Read(Q::Saves+Q::Stride)==original_save);
        assert(Q::start_error==Q::FileError::Incompatible && Q::PromptMessage()==OOT3D::LoadingPrompt::IncompatibleMessageId);
        assert(!Q::starting);
        assert(r[0]==Q::RejectionSound && r[15]==0x4301c4);
        start_rejection.Handle(m,r,5,now,invalid_file);assert(r[15]==0x4336f8);
        assert(!start_rejection.Dispatch(m,r,now+16667,invalid_file));
        assert(std::abs(Q::StartShake())>0 && !Q::choosing && !Q::loading);
        std::array<OOT3D::FileSelect::Rect,OOT3D::FileSelect::Count> bounds{};
        for(auto& b:bounds)b={98,100,180,130};
        const auto pose=OOT3D::FileSelect::PartTransform(OOT3D::FileSelect::Layout{},OOT3D::FileSelect::Start,bounds);
        const auto copy_pose=OOT3D::FileSelect::PartTransform(OOT3D::FileSelect::Layout{},OOT3D::FileSelect::Copy,bounds);
        assert(std::abs(pose.x-40-Q::StartShake())<.0001f && copy_pose.x==40);
        // Repeating A cannot restart the sound/shake every update.
        start_rejection.Handle(m,r,9,now+16667,invalid_file);assert(r[15]==Q::FilePresentation);
        m.words[0x50af18]=0;start_rejection.Dispatch(m,r,now+Q::ShakeUs,invalid_file);
        assert(Q::StartShake()==0 && Q::start_error==Q::FileError::Incompatible);
        m.words[Q::Owner+0x18]=2;start_rejection.Dispatch(m,r,now+Q::ShakeUs+1,invalid_file);
        assert(Q::start_error==Q::FileError::None && Q::PromptMessage()==0x950); // Erase remains accessible.
        m.words[Q::Owner+0x18]=0;invalid_file.error=Q::FileError::Missing;
        start_rejection.Handle(m,r,9,now+Q::ShakeUs+2,invalid_file);
        assert(Q::PromptMessage()==OOT3D::LoadingPrompt::MissingMessageId);
        m.words[Q::Owner+0x10]=9;start_rejection.Dispatch(m,r,now+Q::ShakeUs+3,invalid_file);
        assert(Q::start_error==Q::FileError::None); // Back restores normal prompt.
        invalid_file.error=Q::FileError::None;r[15]=Q::Sites[9].pc+4;
        start_rejection.Handle(m,r,9,now+Q::ShakeUs+4,invalid_file);
        assert(r[15]==Q::Sites[9].pc+4 && r[3]==m.Read(0x430458));
        assert(!Q::starting && Q::PromptMessage()==0x950);
        for(unsigned missing=0;missing<2;++missing) {
            const auto offset=messages->file_error_offsets[missing];
            for(unsigned language:{1u,4u,5u,6u,7u}) {
                const auto text=OOT3D::Messages::Read32(archive,offset+16+language*8);
                assert(archive[text+4]==0x7f && archive[text+5]==0x1d && archive[text+6]==0x41);
            }
            constexpr U32 table=0x9800000,owner=0x9900000;
            auto lookup=r;lookup[0]=owner;lookup[1]=missing?OOT3D::LoadingPrompt::MissingMessageId:OOT3D::LoadingPrompt::IncompatibleMessageId;
            assert(OOT3D::LoadingPrompt::Select(m,lookup) && lookup[0]==table+offset);
        }
    }
    {
        namespace L=OOT3D::LoadingPrompt;
        namespace J=OOT3D::RoomJoin;
        // Solo starts keep the native file label. Only a room's host and
        // automatic guest start use the pulsing prompt through both fades.
        Q::Hook start;
        m.words[Q::Owner+0x14]=2;m.words[Q::Owner+0x10]=8;
        start.Handle(m,r,9,now);
        assert(!Q::starting && Q::PromptMessage()==0x950);
        J::hosting=true;
        start.Handle(m,r,9,now);
        assert(Q::starting && Q::PromptMessage()==L::StartingMessageId);
        J::hosting=false;J::guest=J::Guest::Starting;
        start.Handle(m,r,9,now);
        assert(Q::starting && Q::PromptMessage()==L::StartingMessageId);
        J::guest=J::Guest::None;
        for(unsigned state:{10u,11u}) {
            m.words[Q::Owner+0x10]=state;
            assert(!start.Dispatch(m,r,now+400000));
            assert(Q::starting && Q::PromptMessage()==L::StartingMessageId);
            assert(Q::cursor_us==now+400000);
        }
        for(unsigned language=0;language<10;++language) {
            const auto at=OOT3D::Messages::Read32(archive,messages->starting_offset+16+language*8);
            const auto label=L::StartingLabel(language);
            assert(std::equal(label.begin(),label.end(),archive.begin()+at+4,
                [](char a,std::uint8_t b){return static_cast<std::uint8_t>(a)==b;}));
        }
        auto lookup=r;lookup[0]=0x9900000;lookup[1]=L::StartingMessageId;
        assert(L::Select(m,lookup) && lookup[0]==0x9800000+messages->starting_offset);
        assert(std::abs(L::PresentationOpacity(400000,true)-.45f)<.0001f);
        assert(L::PresentationOpacity(800000,true)==L::PresentationOpacity(0,true));
        assert(L::Opacity(1000000)==L::PresentationOpacity(400000,true));
        // Pulsing only multiplies native text opacity, preserving its fade.
        auto draw=std::make_unique<OOT3D::FileSelect::Hook>();
        draw->stage=OOT3D::FileSelect::Hook::Stage::Native;
        constexpr U32 controller=0x08640000,model=0x08650000,material=0x08660000;
        m.Allocate(controller,0x4c);m.Allocate(material,4);
        m.words[0x55b548+6*4]=controller;m.words[controller+0x40]=3;m.words[controller+0x10]=model;
        Q::cursor_us=400000;m.words[material]=std::bit_cast<U32>(.4f);
        draw->Material(m,material,model);
        assert(std::abs(std::bit_cast<float>(*m.Read(material))-.18f)<.0001f);
        m.words[Q::Owner+0x10]=3;start.Dispatch(m,r,now+800000);
        assert(!Q::starting && Q::PromptMessage()==0x950);
        m.words[0x55b548+6*4]=0;
    }
    {
        namespace J=OOT3D::RoomJoin;
        namespace F=OOT3D::FileSelect;
        struct JoinFiles:Q::OrdinaryFiles {
            unsigned* generated;
            int Prepare(Memory&,unsigned,bool) {++*generated;return 1;}
        };
        Q::Hook join;unsigned generations=0;JoinFiles file{{},&generations};
        m.Allocate(0x55b5e0,0x48);
        m.words[Q::Owner+0x10]=3;m.words[Q::Owner+0x14]=1;
        m.words[J::NameOwner+4]=0;m.words[Q::Exists+4]=0;
        m.words[0x50af14]=m.words[0x50af30]=m.words[0x50af2c]=0;
        std::array<unsigned,3> crcs;
        for(unsigned i=0;i<3;++i)crcs[i]=CRC(m,Q::Saves+i*Q::Stride);
        now=100000000;join.Handle(m,r,2,now,file);join.Handle(m,r,4,now,file);
        m.words[0x50af18]=0;now+=Q::SlideUs+2*Q::StaggerUs;join.Dispatch(m,r,now,file);
        now+=Q::SlideUs+Q::StaggerUs;join.Dispatch(m,r,now,file);assert(Q::ready);
        OOT3D::Randomizer::FileStatus::enabled=false;
        m.words[0x50af18]=0x400;join.Dispatch(m,r,now,file);
        assert(!J::Active() && !join.confirmed);
        m.words[0x50af18]=0;join.Dispatch(m,r,now+1,file);
        OOT3D::Randomizer::FileStatus::enabled=true;
        m.words[0x50af18]=0x400;join.Dispatch(m,r,now+2,file);
        assert(J::phase==J::Phase::ToName && J::slot==1 && join.confirmed && !generations);
        join.Handle(m,r,4,now+2,file);
        m.words[0x50af18]=0;now+=Q::SlideUs+Q::StaggerUs+2;join.Dispatch(m,r,now,file);
        assert(join.awaiting_name && r[15]==Q::Sites[4].pc);
        m.words[Q::Owner+0x10]=5;join.Dispatch(m,r,now,file);
        assert(m.Read(Q::Owner+0x28)==5 && m.Read(Q::Owner+0x54)==0);
        // Native keyboard cleanup returns to the normal file initializer.
        J::phase=J::Phase::ReturnToFiles;J::name=u"Player";
        m.words[Q::Owner+0x10]=2;join.Dispatch(m,r,now,file);
        assert(m.Read(Q::Owner+0x28)==10);
        m.words[Q::Owner+0x10]=3;m.words[0x50af18]=2;
        join.Dispatch(m,r,now,file);
        assert(J::phase==J::Phase::Waiting && J::opacity==0);
        assert(Q::PromptMessage()==OOT3D::LoadingPrompt::WaitingMessageId);
        const auto request=std::atomic_load(&J::request);
        assert(request && request->slot==1 && request->name==u"Player");
        join.Dispatch(m,r,now+75000,file);assert(J::opacity==.5f); // Held B cannot cancel.
        m.words[0x50af18]=0;join.Dispatch(m,r,now+80000,file);
        m.words[0x50af18]=2;join.Dispatch(m,r,now+90000,file);
        const float leaving_from=J::opacity.load();assert(J::phase==J::Phase::Leaving);
        assert(!std::atomic_load(&J::request));join.Handle(m,r,6,now+90000,file);
        join.Dispatch(m,r,now+100000,file);assert(J::opacity<leaving_from);
        join.Dispatch(m,r,now+240000,file);
        assert(!J::Active() && !join.modal && !Q::choosing && Q::returning && join.slot==3);
        assert(r[15]==Q::FilePresentation);
        assert(Q::PromptMessage()==0x950 && m.Read(Q::Owner+0x14)==1);
        for(unsigned i=0;i<3;++i) {
            assert(std::bit_cast<float>(*m.Read(0x55b5f8+i*4))==16);
            assert(std::bit_cast<float>(*m.Read(0x55b604+i*4))==48.f+52*i);
            assert(m.Read(0x55b610+i*4)==0 && m.Read(0x55b61c+i*4)==0);
        }
        assert(join.Dispatch(m,r,now+340000,file) && r[15]==Q::FilePresentation);
        assert(join.Dispatch(m,r,now+800000,file)); // Held B cannot back out again.
        assert(!Q::returning && !Q::choosing && join.block_cancel);
        m.words[0x50af18]=0;assert(!join.Dispatch(m,r,now+820000,file));
        assert(!join.block_cancel && !generations);
        assert(request->cancelled && !request->accepted);
        // A saved host file reloads its native card and selects Start/Copy/Erase.
        // Connection ownership transfers only after that file can be opened.
        J::Begin(1,0);J::name=u"Link";J::Wait(now+830000,false);
        const auto connected=std::atomic_load(&J::request);
        connected->saved=true;
        connected->status=J::Status::Connected;
        join.Dispatch(m,r,now+840000,file);
        assert(Q::PromptMessage()==OOT3D::LoadingPrompt::WaitingMessageId-2);
        assert(!connected->accepted && J::phase==J::Phase::Leaving);
        for(unsigned language:{1u,5u,7u})for(auto status:{J::Status::Receiving,J::Status::ReadyToSave,J::Status::Saving,J::Status::Connected})
            assert(J::Label(status,language)==J::Label(J::Status::Connecting,language));
        join.Dispatch(m,r,now+990000,file); // Only the ordinary 150ms fade, no success pause.
        assert(J::phase==J::Phase::Reopen && !connected->cancelled && m.Read(Q::Owner+0x10)==1);
        m.words[Q::Owner+0x10]=2;join.Dispatch(m,r,now+1000000,file);
        assert(m.Read(Q::Owner+0x28)==10);
        m.words[Q::Owner+0x10]=3;m.words[Q::Exists+4]=1;
        join.Dispatch(m,r,now+1010000,file);
        assert(!J::Active() && connected->accepted && !connected->cancelled && r[4]==1 && r[15]==Q::Sites[2].pc+4);
        assert(J::success_chime && J::entrance_slot==1 && J::HideFiles());
        OOT3D::FileSelect::NameSaveFlow entrance;
        m.words[Q::Owner+0x10]=7;
        assert(!entrance.FileDispatch(m,r) && entrance.returning && entrance.slot==1);
        entrance.Frame();assert(entrance.opacity==0 && J::entrance_slot==3);
        m.words[Q::Owner+0x10]=8;
        assert(entrance.FileDispatch(m,r) && entrance.entering && !entrance.returning);
        for(unsigned frame=0;frame<entrance.EntranceFrames;++frame) {
            assert(entrance.FileDispatch(m,r));entrance.Frame();assert(entrance.opacity==0);
        }
        assert(!entrance.FileDispatch(m,r) && !entrance.entering);
        assert(J::guest==J::Guest::Waiting && J::GuestPrompt());
        assert(J::Label(J::CurrentStatus())=="Waiting for host to start...");
        assert(OOT3D::LoadingPrompt::PresentationOpacity(0)>OOT3D::LoadingPrompt::PresentationOpacity(1000000));
        for(unsigned input:{0u,1u,4u,0x10u,0x20u,0x40u,0x80u}) {
            m.words[0x50af18]=input;assert(join.Dispatch(m,r,now+1100000,file));
            assert(m.Read(Q::Owner+0x10)==8 && r[15]==Q::FilePresentation);
        }
        J::remote_started=true;m.words[0x50af18]=2;
        assert(join.Dispatch(m,r,now+1110000,file)); // B wins a simultaneous host start.
        assert(J::guest==J::Guest::Disconnecting && J::leave_requested && !J::leave_complete);
        assert(J::Label(J::CurrentStatus())=="Disconnecting...");
        assert(OOT3D::LoadingPrompt::PresentationOpacity(0)==1 && OOT3D::LoadingPrompt::PresentationOpacity(1000000)==1);
        assert(join.Dispatch(m,r,now+1200000,file) && r[15]==Q::FilePresentation);
        J::leave_requested=false;J::leave_complete=true;
        assert(join.Dispatch(m,r,now+1210000,file) && r[15]==Q::GuestBack && r[7]==1);
        J::FileSession session;session.Frame(9);assert(!J::leave_requested && J::GuestPrompt());
        session.Frame(3);assert(!J::GuestActive());
        J::guest=J::Guest::Waiting;J::remote_started=true;J::back_held=false;m.words[0x50af18]=0;
        assert(join.Dispatch(m,r,now+1220000,file) && r[15]==Q::GuestStart);
        assert(J::guest==J::Guest::Starting && !J::GuestPrompt() && !J::leave_requested);
        J::Reset();
        m.words[Q::Exists+4]=0;
        // Cancelling at name entry takes the same ordinary file-list path.
        J::Begin(2,1);J::phase=J::Phase::Naming;
        m.words[Q::Owner+0x10]=2;join.Dispatch(m,r,now+900000,file);
        m.words[Q::Owner+0x10]=3;join.Dispatch(m,r,now+916667,file);
        assert(!J::Active() && !Q::choosing && !join.modal && Q::returning);
        assert(r[15]==Q::FilePresentation);
        assert(m.Read(Q::Owner+0x14)==2 && Q::PromptMessage()==0x950);
        assert(join.Dispatch(m,r,now+1016667,file) && r[15]==Q::FilePresentation);
        assert(!join.Dispatch(m,r,now+1500000,file));
        for(unsigned i=0;i<3;++i)assert(crcs[i]==CRC(m,Q::Saves+i*Q::Stride));
        // Existing occupied files enter nickname entry without changing save data.
        std::atomic_store(&J::profiles[1],std::make_shared<const J::Profile>(J::Profile{"test-game",false}));
        m.words[Q::Exists+4]=1;m.words[Q::Owner+0x10]=8;m.words[Q::Owner+0x14]=1;
        m.words[0x50af18]=0;join.Dispatch(m,r,now+2000000,file);
        m.words[Q::Owner+0x18]=2;
        m.words[0x50af18]=0x800;join.Dispatch(m,r,now+2000001,file);
        assert(m.Read(Q::Owner+0x18)==0);
        assert(!join.pending_profile && r[15]==0x4301c4 && r[0]==Q::RandomizerToggleSound);
        const auto hosting_request=std::atomic_load(&J::request);
        assert(hosting_request && hosting_request->inline_host && hosting_request->game_id=="test-game");
        join.Handle(m,r,5,now+2000001,file);
        join.Dispatch(m,r,now+2100000,file);assert(!J::Active());
        join.Dispatch(m,r,now+2200000,file);
        assert(!J::Active() && J::host_opening && m.Read(Q::Owner+0x10)==8 && r[15]==Q::FilePresentation);
        assert(J::HostPrompt() && J::CurrentStatus()==J::Status::Hosting && J::Searching());
        hosting_request->status=J::Status::Connected;J::hosting=true;
        join.Dispatch(m,r,now+2210000,file);
        assert(hosting_request->accepted && !J::host_opening && !J::Active());
        // Hosting locks both disabled actions, while Start and Back still work.
        for(unsigned selection:{1u,2u}) {
            m.words[Q::Owner+0x18]=selection;m.words[0x50af18]=0x80;
            join.Dispatch(m,r,now+2220000,file);
            assert(m.Read(Q::Owner+0x18)==0 && r[15]==Q::FilePresentation);
        }
        m.words[0x50af18]=1;join.Dispatch(m,r,now+2230000,file);
        assert(r[15]==Q::GuestStart);
        m.words[0x50af18]=2;join.Dispatch(m,r,now+2240000,file);
        assert(r[15]==Q::GuestBack && r[7]==1);
        assert(Q::Occupied(m,1) && !generations);
        for(unsigned i=0;i<3;++i)assert(crcs[i]==CRC(m,Q::Saves+i*Q::Stride));
        J::Reset();join.pending_profile=0;J::hosting=true;m.words[0x50af18]=0;
        join.Dispatch(m,r,now+2300000,file);
        assert(J::HostPrompt() && J::Label(J::CurrentStatus())=="Waiting for players to join...");
        m.words[0x50af18]=0x400;join.Dispatch(m,r,now+2400000,file);
        assert(!J::Active() && !join.pending_profile); // Join is disabled while hosting.
        m.words[0x50af18]=0x800;join.Dispatch(m,r,now+2500000,file);
        assert(J::stopping && J::leave_requested && !J::leave_complete);
        assert(!J::HostPrompt() && !J::StatusPrompt() && F::TransitionPromptSlot(8)==7);
        J::hosting=false;J::stopping=false;J::leave_requested=false;m.words[0x50af18]=0;
        join.Dispatch(m,r,now+2600000,file);
        m.words[0x50af18]=0x400;join.Dispatch(m,r,now+2700000,file);
        assert(join.sound_only==Q::Hook::Sound::Select && r[15]==Q::Sites[2].pc+4);
        join.Handle(m,r,4,now+2700000,file);join.Dispatch(m,r,now+2900000,file);
        assert(J::intent==J::Intent::Resume && J::game_id=="test-game");
        assert(J::phase==J::Phase::ProfileLeaving && !join.awaiting_name && !std::atomic_load(&J::request));
        assert(J::name==Q::SavedFileName(m,1));
        assert(join.Dispatch(m,r,now+2975000,file) && r[15]==Q::FilePresentation && J::opacity==.5f);
        assert(join.Dispatch(m,r,now+3050000,file) && r[15]==Q::ProfileBack && r[7]==1);
        assert(J::phase==J::Phase::ProfileReturn && J::HideFiles());
        m.words[Q::Owner+0x10]=9;assert(!join.Dispatch(m,r,now+3060000,file));
        m.words[Q::Owner+0x10]=3;m.words[J::NameOwner+4]=0;
        assert(join.Dispatch(m,r,now+3200000,file) && J::phase==J::Phase::Waiting);
        assert(Q::Occupied(m,1));
        const auto resume_request=std::atomic_load(&J::request);
        assert(resume_request && resume_request->name==Q::SavedFileName(m,1) && resume_request->game_id=="test-game");
        assert(J::CurrentStatus()==J::Status::Matching && J::Label(J::CurrentStatus())=="Looking for matching room ID...");
        for(unsigned i=0;i<3;++i)assert(crcs[i]==CRC(m,Q::Saves+i*Q::Stride));
        J::Reset();J::profile_visible=false;
        std::atomic_store(&J::profiles[1],std::shared_ptr<const J::Profile>{});
        m.words[Q::Owner+0x10]=3;m.words[Q::Exists+4]=0;m.words[0x50af18]=0;
        OOT3D::Randomizer::FileStatus::Publish(1,"seed","ba7ab566-c1e4-4933-9e6d-efdc6251a7bb");
        assert(OOT3D::Randomizer::FileStatus::ShortRoomId(1)=="BA7AB566");
        assert(OOT3D::Randomizer::FileStatus::ShortRoomId(0).empty());
        // Text and X use this same fade amount. Toggling off midway reverses
        // smoothly, and a fresh visit starts with both completely transparent.
        Q::leaving=false;Q::motion_us=150000;F::JoinPromptFade fade;
        fade.Frame(true,false);assert(fade.Alpha()==0);
        for(unsigned i=0;i<4;++i)fade.Frame(true,true);
        const auto middle=fade.Alpha();assert(middle>0 && middle<1);
        fade.Frame(true,false);assert(fade.Alpha()<middle);
        for(unsigned i=0;i<12;++i)fade.Frame(true,true);
        assert(fade.Alpha()==1);
        fade.Frame(false,true);assert(fade.Alpha()==0);
        F::Layout l;l.join_prompt={25,-18,1.5f};
        const auto p=F::JoinPrompt(l),base=F::JoinPrompt(F::Layout{});
        assert(p.x==base.x+25 && p.y==base.y-18 && p.scale==1.5f);
        const auto back=F::WaitingPrompts(F::Layout{},OOT3D::PausePrompts::Assets{},.4f);
        assert(back.icons.count==1 && back.icons.positions[0]==F::ScreenLeft()+10 && back.icons.positions[1]==220);
        assert(back.icons.colors[3]==.4f && back.icons.uvs[0]==144.5f/256);
        // Exercise the real file atlas: waiting retains only the original
        // prompt frame, with the same stationary pose at every fade fraction.
        constexpr U32 donor=0x18000000,mesh=0x18010000,helper=0x18020000;
        constexpr U32 positions=0x18030000,uvs=0x18040000,colors=0x18050000;
        m.words[F::Owner+8]=donor;m.words[F::Owner+4]=mesh;m.words[F::Owner+0x30]=helper;
        m.words[donor]=0x4ebd60;m.words[donor+4]=mesh;m.words[helper]=F::QuadCount;
        m.words[helper+0x10]=positions;m.words[helper+0x14]=uvs;m.words[helper+0x18]=colors;
        F::Geometry raw;
        for(unsigned q=0;q<F::QuadCount;++q)raw.Add(16,16,288,32,0,1,1,0);
        for(unsigned i=0;i<raw.positions.size();++i)m.words[positions+i*4]=std::bit_cast<U32>(raw.positions[i]);
        for(unsigned i=0;i<raw.uvs.size();++i)m.words[uvs+i*4]=std::bit_cast<U32>(raw.uvs[i]);
        for(unsigned i=0;i<raw.colors.size();++i)m.words[colors+i*4]=std::bit_cast<U32>(raw.colors[i]);
        J::phase=J::Phase::Waiting;J::opacity=1;
        const auto full=F::Atlas(m,false);assert(full.count==F::QuadCount);
        J::opacity=.5f;const auto half=F::Atlas(m,false);
        for(unsigned q=0;q<F::QuadCount;++q) {
            for(unsigned i=0;i<12;++i)assert(half.positions[q*12+i]==(q<6?full.positions[q*12+i]:0));
            if(q<6)for(unsigned v=0;v<4;++v)assert(half.colors[q*16+v*4+3]==.5f*full.colors[q*16+v*4+3]);
        }
        J::phase=J::Phase::ReturnToFiles;
        const auto hidden=F::Atlas(m,false);
        for(const auto v:hidden.positions)assert(v==0);
        J::Reset();
        m.words[Q::Owner+0x10]=8;
        const auto normal=F::Atlas(m,false);
        J::guest=J::Guest::Waiting;const auto guest=F::Atlas(m,false);
        for(unsigned q=0;q<F::QuadCount;++q) {
            const auto part=F::QuadPart(q);
            for(unsigned v=0;v<12;++v)assert(guest.positions[q*12+v]==
                (part>=F::Start && part<=F::Erase?0:normal.positions[q*12+v]));
        }
        const auto guest_back=F::Prompts(F::Layout{},OOT3D::PausePrompts::Assets{},.5f,true);
        assert(guest_back.icons.count==1 && guest_back.icons.uvs[0]==144.5f/256);
        assert(guest_back.icons.positions[0]==F::Layout{}.prompts[1].x && guest_back.icons.colors[3]==.5f);
        J::Reset();
        OOT3D::Randomizer::FileStatus::enabled=false;
    }
    std::cout<<"Join room: enabled-only X, no seed/save creation, keyboard handoff, waiting fade, B cancellation, independent prompt pose passed\n";
    std::cout<<"Randomizer errors: native red prompts, EN/FR/ES glyphs and width, async rejection, both quest shakes, retry/cancel and multiplayer failure passed\n";
    std::cout<<"MQ rejection: configuration matrix, red native message, keyboard error cue, held-input guard, shake and recovery passed\n";
    std::cout<<"Master Quest: ROM signatures, three slots, title bypass, creation/cancel, per-file CRC, copy/erase/load and native sounds, animated/shadowed choice, MQ badges and native atlas passed\n";
}
