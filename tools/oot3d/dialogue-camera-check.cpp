// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dialogue_camera.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D::DialogueCamera;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void PoseAt(U32 view,const Pose& p) {
        words[view+0x10]=std::bit_cast<U32>(p.fov);
        for(unsigned i=0;i<3;++i) {
            words[view+0x30+4*i]=std::bit_cast<U32>(p.eye[i]);
            words[view+0x3c+4*i]=std::bit_cast<U32>(p.at[i]);
            words[view+0x48+4*i]=std::bit_cast<U32>(p.up[i]);
        }
    }
};
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(CodeMatches(m));
    for(unsigned i=0;i<Sites.size();++i) {
        assert(Instruction(Sites[i].pc,Sites[i].instruction)==(0xef000000|(FirstSVC+i)));
        assert(Instruction(Sites[i].pc,0)==0);
    }
    m.words[Sites[1].pc+4]^=1;assert(!CodeMatches(m));m.words.clear();
    assert(Sanitize(-1,.5f)==0 && Sanitize(9,.5f)==2 && Sanitize(NAN,.5f)==.5f);
    // Quadrants and wraparound follow MM's signed binary-angle comparisons.
    assert(DialogueYaw(0,0x2000,0x1000)==0x1000);
    assert(DialogueYaw(0,-0x2000,0x1000)==-0x1000);
    assert(DialogueYaw(0,0x6000,0x1000)==-0x7000);
    assert(DialogueYaw(0,-0x6000,0x1000)==0x7000);
    for(int yaw:{-32000,-16000,0,16000,32000})for(int turn:{-25000,-8000,8000,25000})
        assert(DialogueYaw(yaw,SignedAngle(yaw+turn),2000)==
               SignedAngle(yaw+DialogueYaw(0,turn,2000)));
    const Pose a{{0,38,-150},{0,38,0},{0,1,0},52};
    Pose b=a;b.eye[0]=100;b.at[0]=100;b.fov=40;
    assert(Camera(a) && Camera(b));
    auto opposite=a;opposite.eye[2]=150;
    auto middle=Blend(a,opposite,.5);
    assert(Camera(middle)); // Half-turn keeps a valid view, never eye == at.
    // Independently adjustable acceleration and braking belong to the same
    // inward movement. Their integrated velocity stays continuous at the join.
    for(double ramp_in:{0.,.1,.3,2.})for(double ramp_out:{0.,.1,.3,2.}) {
        const double total=ramp_in+ramp_out;if(total==0)continue;
        double previous=0;
        for(unsigned i=0;i<=100;++i) {
            const auto p=Progress(total*i/100.,ramp_in,ramp_out);
            assert(std::isfinite(p) && p>=previous && p<=1);previous=p;
        }
        constexpr double epsilon=1e-5;
        if(ramp_in>0)assert(Progress(epsilon,ramp_in,ramp_out)/epsilon<.001);
        if(ramp_out>0)assert((1-Progress(total-epsilon,ramp_in,ramp_out))/epsilon<.001);
        if(ramp_in>0 && ramp_out>0) {
            const double at=Progress(ramp_in,ramp_in,ramp_out);
            const double before=(at-Progress(ramp_in-epsilon,ramp_in,ramp_out))/epsilon;
            const double after=(Progress(ramp_in+epsilon,ramp_in,ramp_out)-at)/epsilon;
            assert(std::abs(before-after)<1e-5);
        }
    }
    assert(Progress(.1,.4,.1)<Progress(.1,.1,.1)); // Longer acceleration starts more gently.
    assert(Progress(.15,.1,.4)<Progress(.15,.1,.1)); // Longer braking extends the approach.
    Transition t;ease_in_time=ease_out_time=.25;
    assert(!t.Observe(a,1,2,0,0,100000,false));
    assert(t.Observe(b,1,2,0,1,133333,true) && t.output.eye==a.eye);
    double last=0;std::array<double,16> positions{};
    for(unsigned i=1;i<=16;++i) {
        t.Observe(b,1,2,0,i+1,133333+i*33333,true);
        const auto x=t.output.eye[0];assert(x>=last && x<=100);positions[i-1]=x;last=x;
        const auto saved=t.output;
        t.Observe(b,1,2,0,i+1,133334+i*33333,true);assert(t.output.eye==saved.eye);
    }
    assert(!t.blending && t.output.eye==b.eye && t.output.fov==b.fov);
    assert(positions[0]<1 && positions[7]-positions[6]>positions[0]);
    assert(100-positions[13]<1); // Smooth acceleration and deceleration.
    assert(!t.Observe(a,1,2,0,18,t.time+33333,false) && t.output.eye==a.eye);
    // Closing dialogue midway through the inward blend also restores native
    // movement immediately; reopening starts from the latest gameplay view.
    t.Observe(b,1,2,0,19,t.time+33333,true);assert(t.output.eye==a.eye);
    t.Observe(b,1,2,0,20,t.time+33333,true);assert(t.blending);
    assert(!t.Observe(a,1,2,0,21,t.time+33333,false) && t.output.eye==a.eye);
    assert(!t.Observe(b,1,3,0,22,t.time+33333,true) && t.output.eye==b.eye); // Scene cut.
    assert(!t.Observe(a,1,3,1,23,t.time+33333,false)); // Savestate epoch.
    assert(!t.Observe(b,1,3,1,60,t.time+333333,true)); // Gap.
    ease_in_time=ease_out_time=0;t.Reset();t.Observe(a,1,2,0,0,100000,false);
    assert(!t.Observe(b,1,2,0,1,133333,true) && t.output.eye==b.eye);
    ease_in_time=DefaultEaseIn;ease_out_time=DefaultEaseOut;

    constexpr U32 play=0x08000000,view=play+0x188,cam=play+0x364,sp=0x0ffff000;
    m.words[0x5043e0]=play;m.words[0x588e3c]=0;m.words[play+0x104]=2;m.words[play+0xa64]=0;
    m.words[cam+0x188]=7;m.words[cam+0x18c]=0;
    m.words[cam+0xd4]=play;m.words[cam+0xd8]=0x09000000;
    m.words[view]=0;m.words[view+4]=400;m.words[view+8]=0;m.words[view+12]=240;
    for(U32 i=sp-Hook::ScratchSize;i<sp+0x80;i+=4)m.words[i]=0;
    m.PoseAt(view,a);
    std::array<U32,16> regs{};for(unsigned i=0;i<16;++i)regs[i]=i;
    regs[4]=view;regs[13]=sp;regs[15]=Sites[0].pc+4;
    std::array<U32,64> fp{};for(unsigned i=0;i<fp.size();++i)fp[i]=i;
    Hook hook;const auto native=m.words;
    hook.Projection(m,regs,fp,0,100000);assert(!hook.apply && m.words==native);
    m.words[cam+0x18c]=3;m.PoseAt(view,b);const auto talking=m.words;
    const auto saved_regs=regs;
    const auto saved_fp=fp;
    hook.Projection(m,regs,fp,1,133333);assert(regs[15]==OOT3D::OrbitCamera::Collision);
    assert(regs[13]==sp-Hook::ScratchSize && regs[0]==cam);
    // Simulate a collision correction, and a native call clobbering volatile registers.
    const Vec clipped{10,38,-80};OOT3D::OrbitCamera::Put(m,regs[13],clipped);
    for(unsigned i:Hook::SavedRegisters)regs[i]=0xbaad;
    for(unsigned i=0;i<16;++i)fp[i]=0xbaad;
    hook.Projection(m,regs,fp,1,133334);
    assert(hook.apply && hook.displayed.eye==clipped && regs[13]==sp && regs[15]==Sites[0].pc+4);
    for(unsigned i:Hook::SavedRegisters)assert(regs[i]==saved_regs[i]);
    for(unsigned i=0;i<16;++i)assert(fp[i]==saved_fp[i]);
    assert(std::bit_cast<float>(fp[16])==a.fov);
    // Game camera and View inputs remain untouched; only projection s16 and
    // the look-at function's temporary stack vectors receive the eased pose.
    for(auto [address,value]:talking)if(address<sp-Hook::ScratchSize || address>=sp)
        assert(m.words[address]==value);
    regs[5]=sp+0x68;regs[6]=sp+0x5c;regs[7]=sp+0x50;
    hook.LookAt(m,regs,1);Vec eye;
    assert(OOT3D::TitleScreen::ReadFloats(m,regs[5],eye) && eye==clipped && regs[1]==regs[5]);
    // A repeated draw cannot advance easing or run the collision call again.
    hook.Projection(m,regs,fp,1,133335);assert(regs[13]==sp && hook.displayed.eye==clipped);
    m.words[play+0xa64]=1;hook.Projection(m,regs,fp,2,166666);assert(!hook.apply);
    assert(std::bit_cast<float>(fp[16])==b.fov);
    // A fresh hook can unwind a collision restored from a savestate.
    m.words[play+0xa64]=0;m.words[cam+0x18c]=0;hook.Projection(m,regs,fp,3,200000);
    m.words[cam+0x18c]=3;hook.Projection(m,regs,fp,4,233333);assert(regs[13]<sp);
    Hook restored;restored.Projection(m,regs,fp,4,233334);assert(regs[13]==sp && restored.apply);
    // Native MM-style session: seed from the displayed gameplay eye/lens,
    // latch the choice until the conversation ends, and never stack the legacy
    // presentation blend over the native angular path.
    m.words=native;
    constexpr U32 modes=0x5192d4;
    m.words[cam+0x188]=7|(1<<16);m.words[0x516ff0+8+4]=modes;
    m.words[modes+3*8+4]=CommonParameters;
    for(unsigned i=0;i<CommonWords.size();++i)m.words[CommonParameters+4*i]=CommonWords[i];
    for(U32 i=0;i<0x50;i+=4)m.words[cam+i]=0;
    for(U32 i=0;i<12;i+=4)m.words[cam+0x80+i]=m.words[cam+0x8c+i]=m.words[cam+0xa4+i]=0;
    m.words[cam+0x144]=std::bit_cast<U32>(25.f);m.words[cam+0x1a4]=0;
    m.words[0x09001710]=m.words[0x09001714]=0;m.words[cam+0x18c]=0;
    Hook modern;regs=saved_regs;regs[4]=view;regs[13]=sp;mm_style=true;
    Pose wide=a;wide.eye[2]=-300;wide.fov=25;m.PoseAt(view,wide);
    modern.Projection(m,regs,fp,0,100000);assert(modern.gameplay_seen);
    m.words[cam+0x18c]=3;regs[0]=cam;modern.NativeEntry(m,regs,1);
    assert(modern.native_active && regs[6]==cam);
    assert(OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==wide.eye);
    assert(m.words[cam+0x144]==std::bit_cast<U32>(52.f));
    for(float height:{44.f,68.f}) {
        m.words[cam]=std::bit_cast<U32>(-15.f);fp[19]=std::bit_cast<U32>(height-15.f);
        m.words[cam+0x2c]=0x3500000a;fp[1]=std::bit_cast<U32>(-10.f);
        modern.NativeParameters(m,regs,fp);
        assert(m.words[cam+0x2c]==0x3500001e && m.words[cam+0x28]==0);
        assert(m.words[cam+4]==std::bit_cast<U32>(80.f));
        assert(std::abs(std::bit_cast<float>(fp[19])-(height-.16f*height-2.72f))<.001f);
    }
    const auto profile=m.words;
    mm_style=false;modern.NativeEntry(m,regs,1);assert(modern.native_active); // Next conversation only.
    OOT3D::OrbitCamera::Hook orbit;
    modern.Projection(m,regs,fp,1,133333,&orbit);assert(!modern.apply && !modern.transition.blending);
    assert(orbit.dialogue.valid && orbit.dialogue.eye==wide.eye && orbit.dialogue.frame==1);
    assert(orbit.dialogue.fov==wide.fov);
    assert(m.words==profile); // Native projection observes rather than rewriting the camera.
    m.words[cam+0x1a4]=1u<<16;const auto mid_talk=m.words;
    Hook resumed;resumed.NativeEntry(m,regs,2);
    assert(resumed.native_active && m.words==mid_talk);
    // The new setting takes effect once gameplay has resumed.
    m.words[cam+0x18c]=0;modern.Projection(m,regs,fp,2,166666);
    m.words[cam+0x18c]=3;m.words[cam+0x1a4]=0;
    modern.NativeEntry(m,regs,3);assert(!modern.native_active);
    const auto untouched=m.words;fp[1]=std::bit_cast<U32>(-7.f);
    modern.NativeParameters(m,regs,fp);assert(m.words[cam+0x28]==fp[1]);
    for(auto [address,value]:untouched)if(address!=cam+0x28)assert(m.words[address]==value);
    mm_style=true;m.words[modes+3*8+4]=0x518114;modern.NativeEntry(m,regs,4);
    assert(!modern.native_active); // Bespoke scene profiles retain their own camera.
    // Reopening TALK during the exit lens recovery must start from that
    // partially widened lens, not jump to the full gameplay reference.
    m.words[modes+3*8+4]=CommonParameters;m.words[cam+0x18c]=0;
    orbit.active=orbit.dialogue_release=true;orbit.dialogue_fov=46.f;
    orbit.owner=cam;orbit.player_owner=0x09000000;orbit.scene=2;
    orbit.epoch=OOT3D::OrbitCamera::reset_epoch.load();
    modern.Projection(m,regs,fp,5,266666,&orbit);
    assert(modern.gameplay_seen && modern.gameplay_pose.fov==46.f);
    m.words[cam+0x18c]=3;regs[0]=cam;modern.NativeEntry(m,regs,6);
    assert(modern.native_active && m.words[cam+0x144]==std::bit_cast<U32>(46.f));
    // Captured NPC request boundary: player flags request TALK while mode is
    // still NORMAL. Native NORMAL moves the eye before the next TALK update.
    // Display the previous view for that request, then seed TALK from it.
    constexpr U32 player=0x09000000;
    m.words[cam+0x18c]=0;m.words[player+4]=0x06000075;
    m.words[player+0x1710]=m.words[player+0x1714]=0;
    m.words[player+0x16f8]=0;m.PoseAt(view,wide);
    Hook request;regs=saved_regs;regs[4]=view;regs[13]=sp;
    request.Projection(m,regs,fp,10,400000);
    assert(request.gameplay_seen && !request.apply);
    m.words[player+4]=0x06000175;m.words[player+0x16f8]=0x0990d2c0;
    m.words[player+0x1710]=0x20000040;m.words[player+0x1714]=0x00100042;
    Pose bumped=wide;bumped.eye[0]+=40;bumped.eye[1]-=22;m.PoseAt(view,bumped);
    request.Projection(m,regs,fp,11,433333);
    assert(request.apply && request.displayed.eye==wide.eye && request.gameplay_seen);
    assert(std::bit_cast<float>(fp[16])==52.f && request.gameplay_frame==11);
    regs[5]=sp+0x68;regs[6]=sp+0x5c;regs[7]=sp+0x50;
    request.LookAt(m,regs,11);
    assert(OOT3D::TitleScreen::ReadFloats(m,regs[5],eye) && eye==wide.eye);
    m.words[cam+0x18c]=3;m.words[cam+0x1a4]=0;regs[0]=cam;
    request.NativeEntry(m,regs,12);
    assert(request.native_active && OOT3D::TitleScreen::ReadFloats(m,cam+0x8c,eye) && eye==wide.eye);
    // Old gameplay history must never hold an unrelated later interaction.
    m.words[cam+0x18c]=0;
    request.Projection(m,regs,fp,20,733333);
    assert(!request.apply);
    std::cout<<"Dialogue easing: independent acceleration/braking, smooth endpoints, native dialogue exit, signatures, rotation, interruptions, scene/epoch/gap resets, zero bypass, collision, temporary-only output and restored calls passed\n";
}
