// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/save_menu.h"
#include "oot3d/hud_fade.h"

namespace OOT3D::SaveMenu {
// Continue the native top-screen blackout from Save's settled darkness.
// Run BEFORE 300620 updates queued models: enqueueing at draw time leaves the
// pooled rectangle's cached matrix at its previous (often bottom-screen) size.
struct QuitBackdrop {
    bool pending=false;
    std::array<U32,16> saved{};
    std::array<U32,64> saved_fp{};
    U32 saved_flags=0,saved_fpscr=0;
    U32 adjusted_model=0;
    float adjusted_alpha=0;
    template<class M> bool ContinueFade(M& m,float base) {
        constexpr unsigned pass=4;
        const U32 used=m.Read(HUDFade::Queue+8).value_or(25);
        const U32 count=m.Read(HUDFade::Queue+0x14+pass*4).value_or(25);
        if(used>24 || count>24)return false;
        for(unsigned i=0;i<count;++i) {
            const U32 model=m.Read(HUDFade::Queue+0x214+pass*0x60+i*4).value_or(0);
            bool pooled=false;
            for(unsigned j=0;j<used;++j)pooled|=m.Read(HUDFade::Queue+0xf4+j*4)==model;
            std::array<float,6> rect{};std::array<float,4> rgba{};
            if(!pooled || m.Read(model)!=0x4ebd60 ||
               !TitleScreen::ReadFloats(m,model+0x3c,rect) ||
               rect!=std::array<float,6>{200,120,0,400,240,1} ||
               !TitleScreen::ReadFloats(m,model+0xf0,rgba) ||
               rgba[0]!=0 || rgba[1]!=0 || rgba[2]!=0 || rgba[3]<0 || rgba[3]>1)continue;
            // Repeated matrix preparation without a new native enqueue must
            // not compound the opacity. A fresh enqueue replaces this color.
            if(model==adjusted_model && rgba[3]==adjusted_alpha)return true;
            adjusted_model=model;adjusted_alpha=base+(1-base)*rgba[3];
            m.Write(model+0xfc,std::bit_cast<U32>(adjusted_alpha));return true;
        }
        return false;
    }
    template<class M,class R,class F>
    void Handle(M& m,R& r,F& fp,U32& flags,U32& fpscr,U32 entry,bool supported) {
        if(pending) {
            r=saved;fp=saved_fp;flags=saved_flags;fpscr=saved_fpscr;
            pending=false;return;
        }
        r[7]=r[0]; // Displaced MOV, before the queue's per-model update loop.
        if(!supported || r[0]!=HUDFade::Queue || !QuitTransition(m) || !HUDFade::CodeMatches(m)) {
            adjusted_model=0;return;
        }
        const float base=BackdropOpacity(m);
        if(ContinueFade(m,base))return; // One blackout; don't stack another one.
        // Native 3339e8 takes a pointer to four floats. Reserve aligned
        // scratch below SP and restore all CPU state after the extra call.
        const U32 scratch=r[13]-16;
        if(r[13]<16 || (scratch&7))return;
        for(U32 at=scratch;at<r[13];at+=4)if(!m.Read(at))return;
        const U32 used=m.Read(HUDFade::Queue+8).value_or(24);
        if(used>=24 || m.Read(HUDFade::Queue+0x24).value_or(24)>=24)return;
        const U32 model=m.Read(HUDFade::Queue+0xf4+used*4).value_or(0);
        if(!TitleScreen::Pointer(model,0x1e4) || m.Read(model)!=0x4ebd60)return;
        saved=r;saved_fp=fp;saved_flags=flags;saved_fpscr=fpscr;pending=true;
        for(unsigned i=0;i<3;++i)m.Write(scratch+i*4,0);
        // The first closed frame can precede native fade construction. Supply
        // its starting darkness in the same pass; subsequent frames reuse the
        // actual scene fade instead. Native update builds a fresh full matrix.
        adjusted_model=model;adjusted_alpha=base;
        m.Write(scratch+12,std::bit_cast<U32>(base));
        r[13]=scratch;r[0]=HUDFade::Queue;r[1]=4;r[2]=scratch;r[3]=9;
        r[14]=entry;r[15]=0x3339e8;
    }
};
}
