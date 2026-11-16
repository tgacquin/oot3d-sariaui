// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/interpolation_pose_hooks.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::Interpolation;
namespace F=Pose::FishingFoliage;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        const auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};
    }
    void MatrixAt(U32 at,const Matrix& mat) {
        for(unsigned i=0;i<12;++i)words[at+4*i]=std::bit_cast<U32>(float(mat[i/4][i%4]));
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Memory code;std::ifstream in(argv[1],std::ios::binary);assert(in);
    U32 word;for(U32 at=0x100000;in.read(reinterpret_cast<char*>(&word),4);at+=4)code.words[at]=word;
    assert(F::CodeMatches(code));
    for(unsigned i=0;i<F::Sites.size();++i) {
        const auto site=F::Sites[i];
        assert(F::Instruction(site.pc,site.instruction)==(0xef000000|F::FirstSVC+i));
        assert(F::Instruction(site.pc,0)==0);
        code.words[site.pc+4]^=1;assert(!F::CodeMatches(code));code.words[site.pc+4]^=1;
    }
    constexpr U32 play=0x8100000,owner=0x8200000,handle=0x8300000,
        renderer=0x8400000,skeleton=0x8500000,wrapper=0x8600000,
        resource=0x8700044,primitive=0x8800000,stack=0x8900000;
    for(unsigned kind=0;kind<2;++kind) {
        Memory m;const U32 offset=kind?0x520:0x4a0;
        const std::string name=kind?"turi_hasu_model":"turi_kusa_model";
        m.words[0x5043e0]=play;m.words[play+0x104]=0x320049; // Native scene word includes adjacent fields.
        m.words[0x5be5b8+0xfc]=play+0x188;
        m.words[play+0x188]=0;m.words[play+0x18c]=400;m.words[play+0x190]=0;m.words[play+0x194]=240;
        m.words[owner]=0xfe;
        m.words[wrapper]=resource;m.words[resource]=0x206c6b73;m.words[resource+8]=1;
        m.words[resource-0x44]=0x20626d63;m.words[resource-0x20]=0x44;
        for(unsigned i=0;i<16;i+=4) {
            U32 value=0;for(unsigned b=0;b<4;++b)if(i+b<name.size())value|=U32(name[i+b])<<(b*8);
            m.words[resource-0x34+i]=value;
        }
        m.words[primitive+0xc]=1<<16;m.words[primitive+0x10]=0x18;m.words[primitive+0x18]=0;
        for(unsigned slot=0;slot<2;++slot) {
            m.words[owner+offset+slot*4]=handle+slot*0x100;
            m.words[handle+slot*0x100+0x14]=renderer+slot*0x1000;
            m.words[renderer+slot*0x1000]=0x4ebd98;
            m.words[renderer+slot*0x1000+8]=skeleton+slot*0x100;
            m.words[skeleton+slot*0x100+4]=wrapper;
            m.words[F::Props+slot*0x40+0x30]=kind+1;
            m.words[F::Props+slot*0x40+0x3c]=std::bit_cast<U32>(1.f);
        }
        enabled=true;present_60=true;Pose::enabled=true;++generation;guest_frames=10;
        Pose::Reset();F::tags={};
        CameraSample camera{Math::Identity(),play,0x320049,generation.load(),100000,true};
        const auto capture=[&](unsigned plant,unsigned slot,double x,bool tagged=true) {
            Publish(camera);m.MatrixAt(stack+0x10,camera.view);
            std::array<U32,16> regs{};for(unsigned i=0;i<16;++i)regs[i]=0xcafe0000+i;
            regs[4]=F::Props+plant*0x40;regs[5]=owner+slot*4;regs[10]=owner;regs[kind?8:7]=slot;
            const auto before=m.words;const auto saved=regs;
            if(tagged) {
                F::tags.Handle(m,regs,kind,camera.time);
                for(unsigned i=0;i<16;++i)assert(regs[i]==(i==0?handle+slot*0x100:saved[i]));
                assert(m.words==before);
            }
            // Native c20..22 bases from animated-surfaces-1794678936584:
            // reed index_offset 3637600; lily 3773288. Exercise their actual
            // scale/orientation while assigning known prop positions.
            auto matrix=Math::Identity();
            if(kind) {
                matrix[0]={-.4893103838,-.0000066005,-.4105367661,x};
                matrix[1]={-.1470220089,.5963611007,.1752229333,165.4334869};
                matrix[2]={.3833079338,.2287324965,-.4568605721,-800.0775757};
            } else {
                matrix[0]={-.3753835857,.0000009272,-.9051527977,x};
                matrix[1]={-.3082022667,.9213513732,.1278181076,196.3562317};
                matrix[2]={.8510656953,.3336556256,-.3529523313,-945.7424316};
            }
            m.MatrixAt(renderer+slot*0x1000+0x90,matrix);
            regs[6]=renderer+slot*0x1000;regs[11]=primitive;regs[13]=stack;
            const auto memory=m.words;
            Pose::Capture(m,regs,camera.time+100);
            assert(m.words==memory && regs[1]==renderer+slot*0x1000+0x90);
            const auto& pairs=Pose::history.Pairs();
            const auto found=std::find_if(pairs.begin(),pairs.end(),[&](const auto& p) {
                return p.current.frame==guest_frames && p.current.key.renderer==regs[6];
            });
            assert(found!=pairs.end());
            if(tagged)assert(found->current.instance==F::Props+plant*0x40 && found->current.instance_owner==owner);
            return *found;
        };
        capture(0,0,10);capture(1,1,30);
        ++guest_frames;camera.time+=33333;
        const auto moved=capture(1,0,32); // First plant culled: second takes its renderer.
        assert(moved.ready && moved.previous.instance==moved.current.instance);
        assert(moved.previous.key.renderer!=moved.current.key.renderer);
        assert(std::abs(moved.half[0][0][3]-31)<1e-6);
        for(const float phase:{.25f,.5f,.75f}) {
            sample_fraction=phase;
            const auto result=Pose::Resolve(Pose::history.Pairs(),moved.current.native,camera,guest_frames,camera.time+100);
            assert(result && std::abs(result->palette[0][0][3]-(30+2*phase))<1e-6);
        }
        sample_fraction=.5f;
        ++guest_frames;camera.time+=33333;
        const auto reappeared=capture(0,0,12);assert(!reappeared.ready); // Visibility gap stays native.
        const auto returned=capture(1,1,34);assert(returned.ready && returned.half[0][0][3]==33);
        // Missing tags must never fall back to pooled renderer matching.
        Pose::Reset();F::tags={};capture(0,0,10,false);
        ++guest_frames;camera.time+=33333;
        assert(!capture(1,0,30,false).ready);

        // Wrong scene/owner/type, invisible props, invalid slots and addresses
        // reject tagging while always replaying the original LDR exactly.
        const auto fixture=m.words;
        for(unsigned reject=0;reject<10;++reject) {
            m.words=fixture;F::Tags tags;
            std::array<U32,16> r{};r[4]=F::Props;r[5]=owner;r[10]=owner;
            switch(reject) {
            case 1:m.words[play+0x104]=72;break;
            case 2:m.words[owner]=1;break;
            case 3:r[4]+=4;break;
            case 4:m.words[F::Props+0x30]=3;break;
            case 5:m.words[F::Props+0x3c]=0;break;
            case 6:m.words[renderer]=0;break;
            case 7:r[kind?8:7]=99;break;
            case 8:r[4]=F::Props+103*0x40;break;
            case 9:m.words[F::Props+0x3c]=0x7fc00000;break;
            }
            const auto before=m.words;const auto saved=r;
            tags.Handle(m,r,kind,camera.time);
            assert(r[0]==handle && m.words==before);
            for(unsigned i=1;i<16;++i)assert(r[i]==saved[i]);
            auto s=returned.current;s.key.renderer=renderer;s.key.skeleton=skeleton;
            s.frame=guest_frames;s.time=camera.time+100;s.instance=s.instance_owner=0;
            tags.Attach(s);assert(bool(s.instance)==!reject);
            if(!reject)for(unsigned stale=0;stale<6;++stale) {
                auto bad=s;bad.instance=bad.instance_owner=0;
                switch(stale) {
                case 0:++bad.frame;break;
                case 1:++bad.camera.epoch;break;
                case 2:bad.time=camera.time-1;break;
                case 3:bad.time=camera.time+50001;break;
                case 4:++bad.key.skeleton;break;
                case 5:++bad.camera.owner;break;
                }
                tags.Attach(bad);assert(!bad.instance);
            }
        }
    }
    std::cout<<"Fishing native signatures, read-only hook ABI, capture tags, pooled-slot reassignment, phase interpolation, visibility gaps and context/lifetime guards passed\n";
}
