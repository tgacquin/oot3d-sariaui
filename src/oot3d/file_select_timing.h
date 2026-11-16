// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/file_select.h"

namespace OOT3D::FileSelectTiming {
using TitleScreen::U32;
constexpr U32 FirstSVC=0xfff1d0;
// Native file-menu dispatch, four stone axes, Back slide, cursor clocks,
// and the file-select sky's timestep scope. No gameplay clock is changed.
constexpr std::array<TitleScreen::Site,38> Sites{{
    {0x42f9d0,0xe5960010,0xe3500030},
    {0x2ee664,0xee311a21,0xed811a00},
    {0x2ee6ac,0xee311a81,0xed821a00},
    {0x2ee6f4,0xee311a81,0xed821a00},
    {0x2ee738,0xee311a21,0xed801a00},
    {0x444e5c,0xee700a81,0xedc50a13},
    {0x444e94,0xee710a20,0xedc50a13},
    {0x2f94d4,0xe2800001,0xe350001e},
    {0x2f9578,0xe2800001,0xe3500010},
    {0x2f96f4,0xe2800001,0xe350001e},
    {0x45d030,0xe5980000,0xe2800c07},
    {0x45dbd4,0xe28ddf49,0xecbd8b08},
    {0x45d15c,0xe19000f3,0xee000a90},
    {0x45d18c,0xe1d001f0,0xee000a10},
    {0x45d3e0,0xe1d001f0,0xee002a10},
    {0x45d1ac,0xe0800002,0xe59f22bc},
    {0x45d414,0xe0800002,0xe1c10aba},
    {0x2f94f8,0xeef80ae0,0xee710a60},
    {0x2f9518,0xeef80ae0,0xee200a80},
    {0x4300ac,0xe5860054,0xe51f12bc}, // Accepted file: native input delay.
    {0x430500,0xe5950010,0xe2500001}, // Action-stone entrance clock.
    {0x430550,0xeeb80ac0,0xed8d0a18}, // Entrance from the left.
    {0x430568,0xeeb80ac0,0xed8d0a18}, // Entrance from the right.
    {0x430900,0xe5860054,0xe3a00000}, // Back from Start/Copy/Erase: input delay.
    {0x430b64,0xed800a00,0xe5950010}, // Returning selected stone's velocity.
    {0x430b68,0xe5950010,0xe2800001}, // Action exit and file return clock.
    {0x430c98,0xeeb80ac0,0xed8d0a18}, // Action stones exit to the right.
    {0x432d9c,0xe3a00001,0xed9f0ac4}, // First Erase accepted; before redundant confirmation.
    {0x432a18,0xe3500005,0xe5840014}, // Cancel return: five native ticks become ten half steps.
    {0x432d8c,0xe3500005,0xba000258}, // Erase departure completion.
    {0x2ed964,0xeeb80ac0,0xed8d0a01}, // Cancel-card native translation.
    {0x2ed9a0,0xeeb80ac0,0xed8d0a01}, // Erase-card native translation.
    {0x4308b4,0xe5860010,0xea000065}, // Accepted Start/Copy/Erase or file-menu Back.
    {0x432c50,0xe5860010,0xea000024}, // Back from first Erase confirmation.
    {0x4321c8,0xe5860010,0xeafffb50}, // Copy target Back (shared state store).
    {0x2d54f0,0x0dc18a03,0x1d819a03}, // Sky material animation speed; EQ only.
    {0x431b1c,0xe5860010,0xea000028}, // First Overwrite accepted, after prompt cleanup.
    {0x431ce0,0xe5863010,0xeafffc8a}, // Cancel/Back from first Overwrite.
}};
inline U32 Instruction(U32 pc,U32 word) {
    for(unsigned i=0;i<Sites.size();++i)
        if(pc==Sites[i].pc && word==Sites[i].instruction)return (word&0xf0000000)|0x0f000000|(FirstSVC+i);
    return word;
}
template<class M> bool CodeMatches(M& m) {
    for(const auto& s:Sites)
        if(m.Read(s.pc)!=s.instruction || m.Read(s.pc+4)!=s.following)return false;
    return FileSelect::CodeMatches(m) && m.Read(0x45d450)==0x51b2f4 &&
        m.Read(0x2ee82c)==0x55b61c;
}
inline bool Interactive(U32 state) {
    // State 10 is the timed fade into an existing save (431008), not input.
    // Running it on every 60 Hz frame halves the sound's time before teardown.
    return state==3 || state==8 || state==14 || state==17 ||
        state==22 || state==37 || state==41;
}
struct Hook {
    bool active=false,half=false,action_clock=false;
    U32 timestep=0,saved_step=0,previous_state=0,sky_materials=0;
    std::array<float,2> sky_remainder{};
    void Frame(bool enabled) {
        const bool was=active;active=enabled;
        half=active && was && !half;
        if(active && FileSelect::exchange.load()!=FileSelect::Exchange::None)
            FileSelect::exchange_frames=std::min(FileSelect::exchange_frames.load()+1,10u);
        if(!active) {FileSelect::exchange=FileSelect::Exchange::None;previous_state=0;sky_remainder={};action_clock=false;FileSelect::action_opening=false;}
    }
    template<class M,class R,class F> void Handle(M& m,R& r,F& f,unsigned site,U32* cpsr=nullptr) {
        if(site==0) {
            r[0]=m.Read(r[6]+0x10).value_or(0);
            const auto exchange=FileSelect::exchange.load();
            if(active && !FileSelect::ExchangeState(exchange,r[0]))FileSelect::exchange=FileSelect::Exchange::None;
            if(active && r[0]!=previous_state) {
                if(r[0]==36)m.Write(0x55b538,exchange==FileSelect::Exchange::Erase?1:2*m.Read(0x55b538).value_or(5));
                // These second phases only replay a slide already shown by the
                // shared clock. Finish that motion, keeping native prompt setup.
                if(r[0]==13 && exchange==FileSelect::Exchange::Copy) {
                    m.Write(r[6]+0x28,4);unsigned row=0;
                    for(unsigned i=0;i<3;++i)if(m.Read(r[6]+0x14)!=i) {
                        m.Write(0x55b5f8+4*i,std::bit_cast<U32>(16.f));
                        m.Write(0x55b604+4*i,std::bit_cast<U32>(56.f+60*row++));
                        m.Write(0x55b610+4*i,0);m.Write(0x55b61c+4*i,0);
                    }
                }
                if(r[0]==7 && exchange==FileSelect::Exchange::BackCopy) {
                    const unsigned selected=m.Read(r[6]+0x14).value_or(3);
                    if(selected<3) {
                        m.Write(0x55b5f8+4*selected,std::bit_cast<U32>(16.f));
                        m.Write(0x55b604+4*selected,std::bit_cast<U32>(56.f));
                        m.Write(0x55b610+4*selected,0);m.Write(0x55b61c+4*selected,0);
                    }
                }
                if(r[0]==38 || r[0]==39)m.Write(r[6]+0x54,0);
            }
            previous_state=r[0];
            if(active && FileSelect::action_opening.load()) {
                if(r[0]==4 || r[0]==7)++FileSelect::action_frames;
                else FileSelect::action_opening=false;
            }
            if(r[0]!=7)action_clock=false;
            // Only timed states run at 30 Hz. Interactive states read every
            // frame, so a brief press on the extra frame cannot be dropped.
            // The normal tail rebuilds geometry/text on all 60 frames.
            if(active && half && r[0]>1 && r[0]!=7 && r[0]!=9 && r[0]!=36 && r[0]!=38 && r[0]!=39 && !Interactive(r[0])) {
                r[14]=0x4336f8;r[15]=0x2ee5e4;
            }
        } else if(site==36) {
            // Only the explicit first Overwrite choice reaches this store.
            // State 24 is the native accepted-copy path, including departure,
            // save I/O and error handling. Do not enter second-confirmation UI.
            const bool accept=active && r[6]==FileSelect::Owner &&
                m.Read(r[6]+0x10)==17 && r[0]==20 && m.Read(r[6]+0x20)==1;
            if(accept) {
                r[0]=24;m.Write(r[6]+0x20,0);
                FileSelect::BeginExchange(FileSelect::Exchange::AcceptOverwrite);half=true;
            }
            m.Write(r[6]+0x10,r[0]);
        } else if(site==37) {
            const auto old=m.Read(r[6]+0x10);
            m.Write(r[6]+0x10,r[3]);
            if(active && r[6]==FileSelect::Owner && old==17 && r[3]==18) {
                FileSelect::BeginExchange(FileSelect::Exchange::BackOverwrite);half=true;
            }
        } else if(site==35) {
            // Cloud texture animation has its own controller rate. It does
            // not use the temporary scene timestep or the integer sky angle.
            // Only the file-scene sky setup gets half steps; frozen material
            // controllers still take the following native NE store of zero.
            const float speed=std::bit_cast<float>(f[17]);
            m.Write(r[1]+0xc,std::bit_cast<U32>(active && sky_materials && r[4]==sky_materials?speed*.5f:speed));
        } else if(site>=32) {
            const auto old=m.Read(r[6]+0x10).value_or(0);
            m.Write(r[6]+0x10,r[0]);
            if(active && r[6]==FileSelect::Owner) {
                using E=FileSelect::Exchange;E next=E::None;
                if(site==32 && old==8)next=r[0]==34?E::Erase:r[0]==12?E::Copy:r[0]==10?E::Start:E::None;
                if(site==33 && old==37 && r[0]==38)next=E::BackErase;
                if(site==34 && old==14 && r[0]==15)next=E::BackCopy;
                if(site==34 && old==14 && r[0]==16)next=E::Overwrite;
                if(next!=E::None) {
                    FileSelect::BeginExchange(next);m.Write(r[6]+0x54,0);half=true;
                }
            }
        } else if(site==28 || site==29) {
            const U32 a=r[0],b=active?10:5,result=a-b;
            if(cpsr)*cpsr=(*cpsr&0x0fffffff)|(result&0x80000000)|
                (result==0?0x40000000:0)|(a>=b?0x20000000:0)|
                ((((a^b)&(a^result))>>3)&0x10000000);
        } else if(site==30 || site==31) {
            float x=float(std::bit_cast<std::int32_t>(f[0]));
            const U32 state=m.Read(FileSelect::Owner+0x10).value_or(0);
            if(active && state>=36 && state<=39) {
                x*=.5f;
                x=state==36?FileSelect::EasePosition(x,500,0,FileSelect::GetLayout().stone_ease_out):
                    state==37?0:FileSelect::EasePosition(x,0,state==38?500.f:-500.f,FileSelect::GetLayout().stone_ease_out);
            } else if(active && state==42) {
                // The redundant second confirmation is bypassed. Its cleanup
                // must not pull the already departed first pair back onscreen.
                x=-500;
            }
            f[0]=std::bit_cast<U32>(x);
        } else if(site==27) {
            r[0]=1; // Displaced MOV.
            if(active && r[6]==FileSelect::Owner && m.Read(r[6]+0x10)==39) {
                // State 39 has already released the first prompt. Skip only
                // creation/input of the second confirmation (states 40/41).
                // State 42 still owns the native erase animation, sound, save
                // operation and its error handling. No save bytes are touched.
                m.Write(r[6]+0x24,0);m.Write(r[6]+0x2c,0);
                m.Write(r[6]+0x54,0);m.Write(0x55b538,0);
                m.Write(r[6]+0x10,42);
                r[15]=0x4336f8;
            }
        } else if(site<=6) {
            const auto word=Sites[site].instruction;
            const unsigned dst=((word>>12)&15)*2+((word>>22)&1);
            const unsigned a=((word>>16)&15)*2+((word>>7)&1);
            const unsigned b=(word&15)*2+((word>>5)&1);
            const float av=std::bit_cast<float>(f[a]),bv=std::bit_cast<float>(f[b]);
            // In these instructions one operand is position and one velocity.
            const float value=av+bv*(active?.5f:1.f);
            f[dst]=std::bit_cast<U32>(value);
        } else if(site<=9) {
            const bool cursor=active && r[4]==m.Read(FileSelect::Owner+0x34).value_or(0);
            r[0]+=!cursor || !half;
        } else if(site==10) {
            r[0]=m.Read(r[8]).value_or(0);
            timestep=0;
            sky_materials=active && TitleScreen::Pointer(r[4],0x800)?r[4]+0x568:0;
            if(active && TitleScreen::Pointer(r[0],0x114)) {
                const auto word=m.Read(r[0]+0x110);
                if(word && (*word&0xffff)==2) {
                    timestep=r[0]+0x110;saved_step=*word&0xffff;
                    m.Write(timestep,(*word&0xffff0000)|1);
                }
            }
        } else if(site==11) {
            r[13]+=0x124;
            sky_materials=0;
            // Restore only our temporary low halfword; retain neighboring data.
            if(timestep) {
                const auto word=m.Read(timestep);
                if(word && (*word&0xffff)==1 && m.Read(0x51b2f4)==timestep-0x110)
                    m.Write(timestep,(*word&0xffff0000)|saved_step);
                timestep=0;
            }
        } else if(site<=14) {
            const U32 at=site==12?r[0]+r[3]:r[0]+0x10;
            const U32 word=m.Read(at&~3u).value_or(0);
            r[0]=U32(std::int32_t(std::int16_t(word>>((at&2)*8))));
            // Keep native rounding before halving an integer sky-angle step.
            // Halving its timestep before rounding can turn 1 into 1 twice.
            if(active && timestep)r[0]=saved_step;
        } else if(site<=16) {
            if(active) {
                auto& residual=sky_remainder[site-15];
                const float step=float(std::bit_cast<std::int32_t>(r[0]))*.5f+residual;
                const auto whole=static_cast<std::int32_t>(step);
                residual=step-whole;r[0]=U32(whole);
            }
            r[0]+=r[2];
        } else if(site<=18) {
            const bool cursor=active && r[4]==m.Read(FileSelect::Owner+0x34).value_or(0);
            f[1]=std::bit_cast<U32>(float(std::bit_cast<std::int32_t>(f[1]))+
                (cursor && half?3.f:0.f));
        } else if(site==19 || site==23) {
            m.Write(r[6]+0x54,active?0:r[0]);
            if(active && site==19) {
                FileSelect::action_opening=MasterQuest::Occupied(m,m.Read(r[6]+0x14).value_or(3));
                FileSelect::action_frames=1;
            }
            // The next frame must initialize the transition immediately,
            // rather than taking the geometry-only half of the dispatch.
            if(active)half=true;
        } else if(site==20) {
            r[0]=m.Read(r[5]+0x10).value_or(0);
            if(active && (FileSelect::action_opening.load() ||
               FileSelect::exchange.load()==FileSelect::Exchange::BackErase ||
               FileSelect::exchange.load()==FileSelect::Exchange::BackCopy))r[0]=1;
            else if(active && !action_clock) {r[0]*=2;action_clock=true;}
        } else if(site==24) {
            m.Write(r[0],f[0]);
            if(active) {
                // Start all three returns together using native positions and
                // velocities. The regular updater still owns the endpoints.
                const unsigned selected=m.Read(FileSelect::Owner+0x14).value_or(3);
                for(unsigned i=0;i<3;++i) {
                    const float target_y=48.f+52*i;
                    // Copy parks its destinations on the right in two rows.
                    // Returning to the file list must use the same left-side,
                    // fixed-row entrance as Erase, not those leftover poses.
                    if(selected<3 && i!=selected) {
                        m.Write(0x55b5f8+i*4,std::bit_cast<U32>(-384.f));
                        m.Write(0x55b604+i*4,std::bit_cast<U32>(target_y));
                    }
                    const float x=std::bit_cast<float>(m.Read(0x55b5f8+i*4).value_or(0));
                    const float y=std::bit_cast<float>(m.Read(0x55b604+i*4).value_or(0));
                    m.Write(0x55b5e0+i*4,std::bit_cast<U32>(16.f));
                    m.Write(0x55b5ec+i*4,std::bit_cast<U32>(target_y));
                    m.Write(0x55b610+i*4,std::bit_cast<U32>((16-x)/5));
                    m.Write(0x55b61c+i*4,std::bit_cast<U32>((target_y-y)/5));
                }
            }
        } else if(site==25) {
            r[0]=m.Read(r[5]+0x10).value_or(0);
            if(active) {
                // Ten 60 Hz steps replace five 30 Hz ticks. Skip the native
                // final-frame repositioning: those cards are already returning.
                ++r[0];m.Write(r[5]+0x10,r[0]);
                r[15]=r[0]<10?0x430c6c:0x430c64;
            }
        } else {
            float offset=float(std::bit_cast<std::int32_t>(f[0]));
            if(active) {
                offset*=.5f;
                offset=FileSelect::EasePosition(offset,site==26?0:offset<0?-400.f:400.f,
                                               site==26?400.f:0,
                                               FileSelect::GetLayout().stone_ease_out);
            }
            f[0]=std::bit_cast<U32>(offset);
        }
    }
};
}
