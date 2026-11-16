// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include <cassert>
#include <iostream>
#include <map>

namespace F=OOT3D::FileSelect;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) const {
        const auto it=words.find(at);
        return it==words.end()?std::nullopt:std::optional<U32>{it->second};
    }
    template<class T> void Floats(U32 at,const T& a) {
        for(unsigned i=0;i<a.size();++i)words[at+4*i]=std::bit_cast<U32>(a[i]);
    }
};
int main() {
    Memory m;
    constexpr U32 cursor=0x8620000,model=0x8630000;
    m.words[F::Owner+0x34]=cursor;m.words[cursor+4]=model;
    m.words[F::Owner+0x10]=14;
    constexpr std::array<std::array<unsigned,2>,3> destinations{{{1,2},{0,2},{0,1}}};
    auto hook=std::make_unique<F::Hook>();hook->stage=F::Hook::Stage::Native;
    F::Layout layout;layout.stone_focus_grow=true;layout.stone_focus_seconds=0;
    layout.stones={13,-7,1.2f};layout.stone_spacing=.85f;
    for(unsigned slot=0;slot<3;++slot)layout.parts[slot]={float(slot*7),float(slot*9),.8f+.2f*slot};
    for(unsigned source=0;source<3;++source) {
        m.words[F::Owner+0x14]=source;
        // The source is parked offscreen; native copy targets occupy two rows.
        hook->bounds[source]={-384,56,-96,100};
        for(unsigned row=0;row<2;++row)hook->bounds[destinations[source][row]]={16,56.f+60*row,304,100.f+60*row};
        F::StoneFocusAnimation focus;
        for(unsigned row:{0u,1u,0u,1u}) {
            m.words[F::Owner+0x1c]=row;const auto before=m.words;
            const auto slot=destinations[source][row];
            assert(F::CopyDestinationSlot(m)==slot && F::FocusedStone(m,false)==slot);
            hook->frame_layout=layout;focus.Apply(hook->frame_layout,F::FocusedStone(m,false));
            for(unsigned i=0;i<3;++i)assert(hook->frame_layout.stone_focus_amount[i]==(i==slot?1.f:0.f));
            const auto actual=hook->ModelTransform(m,model);
            const auto expected=F::PartTransform(hook->frame_layout,slot,hook->bounds);
            assert(actual.sx>0 && actual.sy>0);
            assert(actual.x==expected.x && actual.y==expected.y && actual.sx==expected.sx && actual.sy==expected.sy);
            assert(actual.ax==expected.ax && actual.ay==expected.ay);
            // The native cursor's row center stays aligned even with individual
            // file poses, group scale/spacing and selected-stone enlargement.
            const float cy=78+60*row;
            assert(std::abs(actual.y+actual.ay+(cy-actual.ay)*actual.sy-
                (expected.y+hook->bounds[slot].Y()))<.001f);
            assert(m.words==before); // Presentation must never change copy targets.
        }
    }
    for(auto invalid:std::array<std::array<unsigned,2>,4>{{{3,0},{0,2},{~0u,0},{0,~0u}}}) {
        m.words[F::Owner+0x14]=invalid[0];m.words[F::Owner+0x1c]=invalid[1];
        assert(F::FocusedStone(m,false)==F::FocusCount && hook->ModelTransform(m,model).sx==0);
    }
    m.words.erase(F::Owner+0x1c);assert(F::CopyDestinationSlot(m)==F::Count);
    m.words[F::Owner+0x10]=3;
    for(unsigned i=0;i<3;++i) {m.words[F::Owner+0x14]=i;assert(F::FocusedStone(m,false)==i);}
    // Both native prompt controllers can coexist during a slide. Route only
    // the destination text from the accepted-input state, including all nine
    // font outline models, without changing native ownership or cleanup.
    hook->bounds[F::Prompt]={12,4,308,44};
    for(unsigned slot:{7u,9u}) {
        const U32 controller=0x8640000+slot*0x100;
        m.words[0x55b548+slot*4]=controller;m.words[controller+0x40]=2;
        for(unsigned j=0;j<9;++j)m.words[controller+0x10+4*j]=0x8650000+slot*0x100+j*4;
    }
    for(unsigned state:{12u,13u,14u,15u,7u}) {
        m.words[F::Owner+0x10]=state;hook->prompt_slot=F::TransitionPromptSlot(state);
        const unsigned expected=state>=12 && state<=14?9:7;
        assert(hook->prompt_slot==expected);
        const auto untouched=m.words;
        for(unsigned slot:{7u,9u})for(unsigned j=0;j<9;++j) {
            const auto pose=hook->ModelTransform(m,0x8650000+slot*0x100+j*4);
            assert((pose.sx>0)==(slot==expected));
        }
        assert(m.words==untouched);
    }
    std::cout<<"Copy prompt: immediate entry/Back text, all outline models, no overlapping labels or native writes passed\n";
    for(unsigned slot:{19u,20u,21u,22u}) {
        const U32 controller=0x8640000+slot*0x100;
        m.words[0x55b548+slot*4]=controller;m.words[controller+0x40]=2;
        for(unsigned j=0;j<9;++j)m.words[controller+0x10+4*j]=0x8650000+slot*0x100+j*4;
    }
    for(unsigned state:{39u,42u}) {
        m.words[F::Owner+0x10]=state;hook->prompt_slot=F::TransitionPromptSlot(state,false);
        assert(hook->prompt_slot==22);
        const auto untouched=m.words;
        for(unsigned slot:{7u,19u,20u,21u,22u})for(unsigned j=0;j<9;++j) {
            const auto pose=hook->ModelTransform(m,0x8650000+slot*0x100+j*4);
            assert((pose.sx>0)==(slot==22));
        }
        assert(m.words==untouched);
    }
    assert(F::TransitionPromptSlot(42,true)!=22);
    assert(F::TransitionPromptSlot(38)!=22 && F::TransitionPromptSlot(37)!=22);
    // A moved/hidden Game Card warning must never move or hide Copy completed.
    // Test both full outline text controllers during every copy/save phase.
    for(unsigned slot:{15u,16u,24u}) {
        const U32 controller=0x8640000+slot*0x100;
        m.words[0x55b548+slot*4]=controller;m.words[controller+0x40]=2;
        for(unsigned j=0;j<9;++j)m.words[controller+0x10+4*j]=0x8650000+slot*0x100+j*4;
    }
    hook->bounds[F::LowerPrompt]={400,195,690,235};
    hook->frame_layout.parts[F::LowerPrompt]={600,350,.5f};
    hook->frame_layout.parts[F::Prompt]={17,-5,1.2f};
    for(unsigned state:{24u,25u,26u,27u,28u,32u}) {
        m.words[F::Owner+0x10]=state;hook->prompt_slot=F::TransitionPromptSlot(state);
        const unsigned expected=state==32?16:15;
        assert(hook->prompt_slot==expected);
        const auto pose=F::PartTransform(hook->frame_layout,F::Prompt,hook->bounds);
        const auto before=m.words;
        for(unsigned slot:{15u,16u})for(unsigned j=0;j<9;++j) {
            const auto actual=hook->ModelTransform(m,0x8650000+slot*0x100+j*4);
            if(slot==expected)assert(actual.x==pose.x && actual.y==pose.y && actual.sx==pose.sx);
            else assert(actual.sx==0);
        }
        assert(m.words==before);
    }
    std::cout<<"Copy completion: correct upper prompt, all text outlines, hidden warning isolation and save-phase coverage passed\n";
    {
        namespace Q=OOT3D::MasterQuest;
        namespace J=OOT3D::RoomJoin;
        namespace L=OOT3D::LoadingPrompt;
        // Hosting creates controller 6, which native Copy/Erase never owned
        // and therefore never destroys. Keep that exact controller alive while
        // moving through every action/confirmation/save/departure state.
        constexpr U32 controller=0x8640600;
        m.words[0x55b548+6*4]=controller;m.words[controller+0x40]=2;
        for(unsigned j=0;j<9;++j)m.words[controller+0x10+4*j]=0x8650600+j*4;
        Q::prompt_controller=controller;Q::start_error=Q::FileError::None;J::Reset();
        for(const auto message:{L::WaitingMessageId-unsigned(J::Status::Hosting),
                               L::WaitingMessageId-unsigned(J::Status::WaitingHost),
                               L::IncompatibleMessageId,L::MissingMessageId}) {
            Q::prompt_message=message;
            for(unsigned state:{8u,10u,12u,13u,14u,15u,16u,17u,18u,24u,25u,26u,27u,28u,
                                32u,34u,36u,37u,38u,39u,40u,41u,42u,44u}) {
                m.words[F::Owner+0x10]=state;hook->prompt_slot=F::TransitionPromptSlot(state,false,1);
                const auto before=m.words;
                for(unsigned j=0;j<9;++j)assert(hook->ModelTransform(m,0x8650600+j*4).sx==0);
                assert(m.words==before);
            }
        }
        // An active hosting message still draws, including its native outline;
        // stopping immediately hides it even before the replacement is rebuilt.
        const auto previous_entry=std::atomic_load(&L::entry);
        std::atomic_store(&L::entry,std::make_shared<const L::Entry>());
        J::profile_visible=true;J::hosting=true;hook->prompt_slot=6;
        Q::prompt_message=L::RoomMessageId();m.words[F::Owner+0x10]=8;
        for(unsigned j=0;j<9;++j)assert(hook->ModelTransform(m,0x8650600+j*4).sx>0);
        J::stopping=true;
        for(unsigned j=0;j<9;++j)assert(hook->ModelTransform(m,0x8650600+j*4).sx==0);
        J::Reset();J::profile_visible=false;
        Q::start_error=Q::FileError::Incompatible;Q::prompt_message=L::IncompatibleMessageId;
        for(unsigned j=0;j<9;++j)assert(hook->ModelTransform(m,0x8650600+j*4).sx>0);
        Q::start_error=Q::FileError::None;Q::prompt_message=0x950;
        m.words[F::Owner+0x10]=3;
        for(unsigned j=0;j<9;++j)assert(hook->ModelTransform(m,0x8650600+j*4).sx>0);
        std::atomic_store(&L::entry,previous_entry);Q::prompt_controller=0;
        std::cout<<"Private prompt lifetime: host/unhost through Copy/Erase/Start, all nine text layers, active-message preservation and no guest writes passed\n";
    }
    {
        constexpr U32 mesh=0x8700000,helper=0x8710000,positions=0x8720000,uvs=0x8730000,colors=0x8740000;
        m.words[F::Owner+8]=model;m.words[F::Owner+4]=mesh;
        m.words[model]=0x4ebd60;m.words[model+4]=mesh;
        m.words[F::Owner+0x30]=helper;m.words[helper]=F::QuadCount;
        m.words[helper+0x10]=positions;m.words[helper+0x14]=uvs;m.words[helper+0x18]=colors;
        for(unsigned source=0;source<3;++source) {
            m.words[F::Owner+0x14]=source;unsigned row=0;
            F::Geometry raw;
            for(unsigned q=0;q<F::QuadCount;++q)raw.Add(0,0,1,1,0,0,1,1);
            for(unsigned slot=0;slot<3;++slot) {
                const float x=slot==source?-384.f:16.f,y=slot==source?56.f:56.f+60*row++;
                m.words[0x55b5f8+4*slot]=std::bit_cast<U32>(x);
                m.words[0x55b604+4*slot]=std::bit_cast<U32>(y);
                // Native 2ed224 puts the visible face two units outside the
                // scalar anchor. Test the real inset, not edge==anchor fixtures.
                for(unsigned q=6+37*slot;q<=8+37*slot;++q) {
                    const std::array<float,12> p{x-2,y-2,0,x+290,y-2,0,x-2,y+46,0,x+290,y+46,0};
                    std::copy(p.begin(),p.end(),raw.positions.begin()+q*12);
                }
            }
            m.Floats(positions,raw.positions);m.Floats(uvs,raw.uvs);m.Floats(colors,raw.colors);
            m.words[F::Owner+0x10]=13;F::exchange=F::Exchange::Copy;F::exchange_frames=10;
            const auto sliding=F::Atlas(m,false,layout);
            m.words[F::Owner+0x10]=14;F::exchange=F::Exchange::None;
            const auto settled=F::Atlas(m,false,layout);
            assert(sliding.count==F::QuadCount && settled.count==F::QuadCount);
            for(unsigned slot=0;slot<3;++slot)if(slot!=source)
                for(unsigned q=6+37*slot;q<=8+37*slot;++q)for(unsigned i=0;i<12;++i)
                    assert(std::abs(sliding.positions[q*12+i]-settled.positions[q*12+i])<.001f);
        }
        m.words[F::Owner+0x10]=4;F::action_opening=true;F::action_frames=3;
        for(unsigned q:{125u,129u,133u})for(unsigned v=0;v<4;++v)
            m.words[colors+q*64+v*16+12]=std::bit_cast<U32>(.7f);
        const auto untouched=m.words;
        const auto opening=F::Atlas(m,false,layout);
        for(unsigned q:{125u,129u,133u})for(unsigned v=0;v<4;++v)
            assert(opening.colors[q*16+v*4+3]==1);
        F::action_opening=false;m.words[F::Owner+0x10]=8;
        const auto resting=F::Atlas(m,false,layout);
        for(unsigned q:{125u,129u,133u})for(unsigned v=0;v<4;++v)
            assert(resting.colors[q*16+v*4+3]==.7f);
        OOT3D::RoomJoin::phase=OOT3D::RoomJoin::Phase::ProfileLeaving;OOT3D::RoomJoin::opacity=.5f;
        const auto leaving=F::Atlas(m,false,layout);
        for(unsigned i=3;i<F::QuadCount*16;i+=4)assert(leaving.colors[i]==resting.colors[i]*.5f);
        OOT3D::RoomJoin::Reset();
        m.words[F::Owner+0x10]=4;assert(m.words==untouched);
        std::cout<<"Copy endpoint: native face inset, edited layouts, all sources; action-entry alpha isolation passed\n";
    }
    std::cout<<"Erase prompt: immediate deletion text, confirmation suppression and cancellation guards passed\n";
    std::cout<<"Copy selection: all three sources, both destinations, initial focus, up/down, cursor pose and growth passed\n";
}
