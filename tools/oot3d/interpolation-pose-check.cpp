// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <filesystem>
#include "oot3d/interpolation_pose_hooks.h"
using namespace OOT3D::Interpolation;
using U32=std::uint32_t;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Put(U32 at,const Matrix& matrix) {
        for(unsigned i=0;i<12;++i)words[at+4*i]=std::bit_cast<U32>(float(matrix[i/4][i%4]));
    }
};
static void CheckLinkHierarchy(const std::filesystem::path& code) {
    const std::array<unsigned,25> parents{65535,0,1,2,3,4,2,6,7,1,9,10,11,
        10,13,14,15,10,17,18,19,10,0,22,22};
    // Verify the actual local ROM models, not an assumed MM bone layout.
    for(const auto name:{"zelda_link_child_new.zar","zelda_link_boy_new.zar"}) {
        std::ifstream in(code.parent_path().parent_path()/"romfs"/"actor"/name,std::ios::binary);
        assert(in);std::string bytes{std::istreambuf_iterator<char>(in),{}};
        const auto cmb=bytes.find("cmb ");assert(cmb!=std::string::npos);
        const auto word=[&](std::size_t at) {
            assert(at+4<=bytes.size());U32 value{};
            for(unsigned b=0;b<4;++b)value|=U32(static_cast<unsigned char>(bytes[at+b]))<<(8*b);
            return value;
        };
        assert(word(cmb+0x24)==0x44);
        const auto skl=cmb+0x44;
        assert(word(skl)==0x206c6b73 && word(skl+4)==1016 && word(skl+8)==25);
        for(unsigned i=0;i<25;++i)assert(word(skl+16+40*i)==(parents[i]<<16|i));
    }
    auto first=std::make_shared<Pose::Skeleton>();first->bones.resize(25);
    for(unsigned i=0;i<25;++i) {
        auto& bone=first->bones[i];bone.parent=parents[i];bone.bind=Math::Identity();
        auto local=Math::Identity();if(i)local[1][3]=10;
        bone.world=i?Math::Multiply(first->bones[parents[i]].world,local):local;
    }
    first->bones[1].bind[1][3]=-10;
    auto turn=Math::Identity();turn[0][0]=0;turn[0][1]=-1;turn[1][0]=1;turn[1][1]=0;
    auto second=std::make_shared<Pose::Skeleton>(*first);
    for(auto& bone:second->bones)bone.world=Math::Multiply(turn,bone.world);
    const auto half=Pose::BlendSkeleton(*first,*second);assert(half.size()==25);
    const auto root_half=*Pose::Half(Math::Identity(),turn);
    for(unsigned i=1;i<25;++i) {
        assert(half[i] && half[parents[i]]);
        double distance=0;for(unsigned a=0;a<3;++a)
            distance+=std::pow((*half[i])[a][3]-(*half[parents[i]])[a][3],2);
        assert(std::abs(std::sqrt(distance)-10)<1e-8);
        assert(Math::Near(*half[i],Math::Multiply(root_half,first->bones[i].world),1e-8));
    }
    for(double fraction:{.25,.75}) {
        const auto quarter=Pose::BlendSkeleton(*first,*second,false,false,fraction);
        const auto root=*Pose::Half(Math::Identity(),turn,fraction);
        for(unsigned i=1;i<25;++i) {
            assert(quarter[i] && quarter[parents[i]]);
            double distance=0;for(unsigned a=0;a<3;++a)
                distance+=std::pow((*quarter[i])[a][3]-(*quarter[parents[i]])[a][3],2);
            assert(std::abs(std::sqrt(distance)-10)<1e-8);
            assert(Math::Near(*quarter[i],Math::Multiply(root,first->bones[i].world),1e-8));
        }
    }
    {
        Pose::PhaseSkeletonCache cache;Pose::Sample old,next;
        old.skeleton_pose=first;next.skeleton_pose=second;next.model="childlink_v2";
        const auto quarter=cache.Get(old,next,.25f);
        assert(quarter==Pose::BlendSkeleton(*first,*second,false,true,.25));
        assert(cache.Get(old,next,.25f)==quarter && cache.builds==1 && cache.hits==1);
        assert(cache.Get(old,next,.75f)==Pose::BlendSkeleton(*first,*second,false,true,.75));
        assert(cache.builds==2);
        next.model="link_v2";cache.Get(old,next,.25f);assert(cache.builds==3);
        next.skeleton_pose=std::make_shared<Pose::Skeleton>(*second);
        cache.Get(old,next,.25f);assert(cache.builds==4); // new snapshot cannot reuse old frame
    }
    const auto old_half=Pose::Half(first->bones[1].world,second->bones[1].world);
    assert(old_half && std::abs(std::hypot((*old_half)[0][3],(*old_half)[1][3])-7.071067811865)<1e-8);
    // Change local joint rotations as well as actor heading, as happens when
    // changing actions. Include Link's .01 world scale and float storage.
    auto posed_a=*first,posed_b=*first;
    for(unsigned i=0;i<25;++i) {
        auto local_a=Math::Identity(),local_b=i%3==0?turn:Math::Identity();
        if(i)local_a[1][3]=local_b[1][3]=10;
        else {
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c) {
                local_a[r][c]*=.01;local_b[r][c]*=.01;
            }
            local_a[0][3]=local_b[0][3]=1000;
        }
        posed_a.bones[i].world=i?Math::Multiply(posed_a.bones[parents[i]].world,local_a):local_a;
        posed_b.bones[i].world=i?Math::Multiply(posed_b.bones[parents[i]].world,local_b):local_b;
        for(auto* pose:{&posed_a,&posed_b})for(auto& row:pose->bones[i].world)
            for(auto& value:row)value=float(value);
    }
    const auto posed_half=Pose::BlendSkeleton(posed_a,posed_b);
    assert(posed_half.size()==25);
    for(unsigned i=1;i<25;++i) {
        assert(posed_half[i] && posed_half[parents[i]]);
        double distance=0;for(unsigned axis=0;axis<3;++axis)
            distance+=std::pow((*posed_half[i])[axis][3]-(*posed_half[parents[i]])[axis][3],2);
        assert(std::abs(std::sqrt(distance)-.1)<.0001);
    }
    // A hidden weapon or flattened band must not freeze the torso/head.
    auto hidden_a=*first,hidden_b=*second;
    for(unsigned r=0;r<3;++r)hidden_a.bones[23].world[r][1]=hidden_b.bones[23].world[r][1]=0;
    const auto flat=Pose::BlendSkeleton(hidden_a,hidden_b);
    assert(flat.size()==25 && flat[23] && flat[11]);
    for(unsigned r=0;r<3;++r)assert((*flat[23])[r][1]==0);
    for(unsigned r=0;r<3;++r)hidden_a.bones[24].world[r][0]=hidden_a.bones[24].world[r][1]=0;
    const auto hidden=Pose::BlendSkeleton(hidden_a,hidden_b);assert(hidden[11] && !hidden[24]);
    auto changed=*second;changed.bones[1].parent=1;assert(Pose::BlendSkeleton(*first,changed).empty());
    changed=*second;changed.bones[0].world[0][3]=10000;assert(Pose::BlendSkeleton(*first,changed).empty());
    changed=*second;changed.bones[1].bind[0][3]+=1;assert(Pose::BlendSkeleton(*first,changed).empty());
    auto scaled=Math::Identity();for(unsigned i=0;i<3;++i)scaled[i][i]=.01;
    scaled[0][3]=1000;const auto inv=Pose::InverseBone(scaled);
    assert(inv && Math::Near(Math::Multiply(*inv,scaled),Math::Identity(),1e-8));
    scaled[0][0]=0;assert(!Pose::InverseBone(scaled));

    Memory m;
    constexpr U32 renderer=0x8100000,skeleton=0x8200000,wrapper=0x8300000,
        resource=0x8400000,primitive=0x8500000,stack=0x8600000,play=0x8700000,
        shape=0x8800000,bone_data=0x8900000,bind_data=0x8a00000;
    m.words[renderer]=0x4ebd98;m.words[renderer+8]=skeleton;m.words[skeleton+4]=wrapper;
    m.words[skeleton+0x14]=bone_data;m.words[skeleton+8]=bind_data;
    m.words[wrapper]=resource;m.words[resource]=0x206c6b73;m.words[resource+4]=1016;m.words[resource+8]=25;
    m.words[primitive+0xc]=2u<<16|2;m.words[primitive+0x10]=0x18;m.words[primitive+0x18]=1u<<16;
    m.words[resource-0x44]=0x20626d63;m.words[resource-0x20]=0x44;
    m.words[0x5043e0]=play;m.words[0x5be5b8+0xfc]=play+0x188;m.words[play+0x104]=2;
    m.words[play+0x188]=0;m.words[play+0x18c]=400;m.words[play+0x190]=0;m.words[play+0x194]=240;
    CameraSample camera{Math::Identity(),play,2,1,100000,true};
    camera.view[0][3]=-30;
    Pose::Sample source;source.key={renderer,skeleton,resource,primitive,2};source.skinning=2;
    source.bone_count=25;source.bone_ids={0,1};source.draw_view=camera.view;
    source.vertex_offset[0][3]=3;source.vertex_offset[1][3]=-2;source.vertex_offset[2][3]=4;
    for(unsigned i=0;i<3;++i)m.words[shape+0x18+i*4]=std::bit_cast<U32>(float(source.vertex_offset[i][3]));
    for(unsigned i=0;i<25;++i) {
        m.words[resource+16+40*i]=parents[i]<<16|i;
        m.Put(bind_data+48*i,first->bones[i].bind);
    }
    std::array<U32,16> regs{};regs[6]=renderer;regs[7]=shape;regs[11]=primitive;regs[13]=stack;
    const auto install=[&](const std::shared_ptr<const Pose::Skeleton>& pose) {
        source.skeleton_pose=pose;
        for(unsigned i=0;i<25;++i)m.Put(bone_data+i*48,pose->bones[i].world);
        m.Put(stack+0x10,source.draw_view);
        for(unsigned i=0;i<source.key.count;++i) {
            const auto world=Pose::SkinBone(source,i,pose->bones[source.bone_ids[i]].world);assert(world);
            source.native[i]=Math::Multiply(source.draw_view,*world);
            m.Put(renderer+0x90+48*i,source.native[i]);
        }
    };
    for(const std::string model:{"childlink_v2","link_v2"}) {
        for(unsigned i=0;i<16;i+=4) {
            U32 word=0;for(unsigned b=0;b<4;++b)if(i+b<model.size())word|=U32(model[i+b])<<(8*b);
            m.words[resource-0x34+i]=word;
        }
        Pose::Reset();++generation;camera.epoch=generation;guest_frames=10;camera.time=100000;
        source.key.count=2;m.words[primitive+0xc]=2u<<16|2;
        Publish(camera);install(first);const auto original=m.words;
        Pose::Capture(m,regs,camera.time);assert(m.words==original);
        assert(Pose::history.Pairs().size()==1);
        const auto captured=Pose::history.Pairs()[0].current;
        assert(captured.model==model && captured.skeleton_pose && Pose::SkeletonMatches(captured));
        // All meshes share the same immutable snapshot in one native frame.
        Pose::Capture(m,regs,camera.time+1);
        assert(Pose::history.Pairs()[0].current.skeleton_pose==captured.skeleton_pose);
        guest_frames=11;camera.time+=33333;Publish(camera);install(second);
        Pose::Capture(m,regs,camera.time);const auto pairs=Pose::history.Pairs();
        assert(pairs[0].ready && pairs[0].current.skeleton_pose!=captured.skeleton_pose);
        const auto resolved=Pose::Resolve(pairs,pairs[0].current.native,camera,11,camera.time);
        assert(resolved);
        for(unsigned i=0;i<2;++i) {
            const auto skin=Pose::SkinBone(source,i,*half[i]);assert(skin);
            assert(Math::Near(resolved->palette[i],Math::Multiply(camera.view,*skin),1e-6));
        }
        // Changed same-frame palettes replace the cache and invalidate replay.
        install(first);Pose::Capture(m,regs,camera.time+2);
        assert(!Pose::history.Pairs()[0].ready);
        assert(Pose::history.Pairs()[0].current.skeleton_pose!=pairs[0].current.skeleton_pose);
        // Native count-one smooth palettes bypass the non-identity bind.
        source.key.count=1;source.bone_ids[0]=1;m.words[primitive+0xc]=1u<<16|2;
        m.words[primitive+0x18]=1;install(first);
        const auto direct=Pose::SkinBone(source,0,first->bones[1].world);
        assert(direct && Math::Near(*direct,Math::Multiply(first->bones[1].world,source.vertex_offset),1e-8));
        Pose::Capture(m,regs,camera.time+3);
        assert(Pose::history.Pairs().back().current.skeleton_pose);
        source.bone_ids={0,1};m.words[primitive+0x18]=1u<<16;
        // Reconstruction mismatch must retain native Link, not the old blend.
        Pose::Reset();source.key.count=2;m.words[primitive+0xc]=2u<<16|2;
        install(first);m.words[renderer+0x90]=std::bit_cast<U32>(5.f);
        Pose::Capture(m,regs,camera.time+4);
        assert(!Pose::history.Pairs()[0].current.skeleton_pose);
        ++guest_frames;camera.time+=33333;Publish(camera);install(second);Pose::Capture(m,regs,camera.time);
        assert(!Pose::history.Pairs()[0].ready);
    }
    auto malformed=source;malformed.bone_ids[0]=25;assert(!Pose::SkeletonMatches(malformed));
    m.words[resource+16+40]=1u<<16|1;assert(!Pose::ReadSkeleton(m,source));
}
int main(int argc,char** argv) {
    assert(argc==2);Memory m;std::ifstream file(argv[1],std::ios::binary);assert(file);U32 word;
    for(U32 a=0x100000;file.read(reinterpret_cast<char*>(&word),4);a+=4)m.words[a]=word;
    assert(Pose::CodeMatches(m));
    assert(Pose::Instruction(Pose::Site.pc,Pose::Site.instruction)==(0xef000000|Pose::SVC));
    assert(Pose::Instruction(Pose::Site.pc,0)==0);
    m.words[Pose::Site.pc+4]^=1;assert(!Pose::CodeMatches(m));m.words[Pose::Site.pc+4]^=1;
    constexpr U32 renderer=0x8100000,skeleton=0x8200000,wrapper=0x8300000,
        resource=0x8400000,primitive=0x8500000,stack=0x8600000;
    m.words[renderer]=0x4ebd98;m.words[renderer+8]=skeleton;m.words[skeleton+4]=wrapper;
    m.words[wrapper]=resource;m.words[resource]=0x206c6b73;m.words[resource+8]=2;
    m.words[primitive+0xc]=2u<<16|2;
    m.words[primitive+0x10]=0x18;m.words[primitive+0x18]=1u<<16;
    m.words[resource-0x44]=0x20626d63;m.words[resource-0x20]=0x44;
    m.words[resource-0x34]=0x74736574;m.words[resource-0x30]=0; // "test"
    std::array<U32,16> regs{};regs[6]=renderer;regs[11]=primitive;regs[13]=stack;
    constexpr U32 play=0x8700000,active_view=play+0x188;
    m.words[0x5043e0]=play;m.words[0x5be5b8+0xfc]=active_view;m.words[play+0x104]=2;
    m.words[active_view]=0;m.words[active_view+4]=400;
    m.words[active_view+8]=0;m.words[active_view+12]=240;
    CameraSample camera{Math::Identity(),play,2,1,100000,true};
    enabled=true;present_60=true;Pose::enabled=true;generation=1;guest_frames=10;
    auto bone=Math::Identity(),second=bone;second[1][3]=30;
    m.Put(stack+0x10,camera.view);m.Put(renderer+0x90,bone);m.Put(renderer+0xc0,second);
    Publish(camera);const auto original=m.words;const auto original_regs=regs;
    Pose::Capture(m,regs,100000);
    assert(m.words==original); // Capture is strictly read-only, including native palette memory.
    for(unsigned i=0;i<16;++i)assert(regs[i]==(i==1?renderer+0x90:original_regs[i]));
    assert(Pose::history.Pairs().size()==1 && !Pose::history.Pairs()[0].ready);
    const auto& metadata=Pose::history.Pairs()[0].current;
    assert(metadata.bone_count==2 && metadata.skinning==2 && metadata.model=="test");
    assert(metadata.bone_ids[0]==0 && metadata.bone_ids[1]==1);
    // Actor moves 20 units and camera moves 10. Blend world poses, then put
    // them back in CURRENT view space. Camera replay handles its own midpoint.
    camera.view[0][3]=-10;camera.time=133333;Publish(camera);guest_frames=11;
    bone[0][3]=10;second[0][3]=10;
    m.Put(stack+0x10,camera.view);m.Put(renderer+0x90,bone);m.Put(renderer+0xc0,second);
    Pose::Capture(m,regs,133333);
    Pose::Palette palette{};palette[0]=bone;palette[1]=second;
    Pose::Replay replay;
    const auto half=replay.Select(palette,camera,11,133333);assert(half && half->count==2);
    assert(std::abs(half->palette[0][0][3])<.000001);
    assert(std::abs(half->palette[1][1][3]-30)<.000001);
    const auto paired=Pose::history.Pairs();
    Pose::Capture(m,regs,133334);assert(Pose::history.Pairs()[0].ready);
    assert(replay.Select(palette,camera,11,133334)->palette==half->palette);
    auto wrong=palette;wrong[1][1][3]+=1;assert(!replay.Select(wrong,camera,11,133334));
    auto other=camera;other.scene++;assert(!replay.Select(palette,other,11,133334));
    other=camera;other.epoch++;assert(!replay.Select(palette,other,11,133334));
    assert(!replay.Select(palette,camera,13,133334));
    assert(!replay.Select(palette,camera,11,233334));
    // Identical native palettes from different actors must not select the
    // wrong actor's intermediate pose.
    auto ambiguous=paired;auto duplicate=paired[0];duplicate.current.key.renderer+=4;
    duplicate.half[0][0][3]+=3;ambiguous.push_back(duplicate);
    assert(!Pose::Resolve(ambiguous,palette,camera,11,133333));
    // Equivalent transforms are safe, but an unpaired lookalike is not.
    ambiguous.back()=paired[0];assert(Pose::Resolve(ambiguous,palette,camera,11,133333));
    ambiguous.back().ready=false;assert(!Pose::Resolve(ambiguous,palette,camera,11,133333));
    // Link's head-area meshes share the skin palette [10,11], while the hat
    // extends it to [10,11,12]. A shorter matching upload is not a conflict
    // when it is the same instance and every shared midpoint agrees.
    auto head=paired[0],hat=head;
    head.current.skinning=hat.current.skinning=2;
    head.current.bone_count=hat.current.bone_count=25;
    head.current.bone_ids={10,11};hat.current.bone_ids={10,11,12};
    hat.current.key.primitive+=0x100;hat.current.key.count=3;
    hat.current.native[2]=Math::Identity();hat.current.native[2][1][3]=70;
    hat.half[2]=Math::Identity();hat.half[2][1][3]=65;
    auto head_palette=palette;head_palette[2]=hat.current.native[2];
    const auto head_original=head_palette;
    for(bool reverse:{false,true}) {
        std::vector<Pose::Pair> overlapping=reverse?std::vector{hat,head}:std::vector{head,hat};
        const auto merged=Pose::Resolve(overlapping,head_palette,camera,11,133333);
        assert(merged && merged->count==3);
        assert(merged->palette[0]==half->palette[0] && merged->palette[1]==half->palette[1]);
        assert(merged->palette[2][1][3]==65);
        assert(head_palette==head_original); // Never change the native upload.
    }
    auto conflict=hat;conflict.half[0][0][3]+=1;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    conflict=hat;conflict.current.key.renderer+=4;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    conflict=hat;conflict.current.bone_ids[0]=9;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    conflict=hat;conflict.current.skinning=0;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    conflict=hat;conflict.current.bone_ids[0]=head.current.bone_ids[0]=65535;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    head.current.bone_ids[0]=10;
    conflict=hat;conflict.ready=false;
    assert(!Pose::Resolve({head,conflict},head_palette,camera,11,133333));
    auto wrong_tail=head_palette;wrong_tail[2][1][3]+=1;
    const auto short_only=Pose::Resolve({head,hat},wrong_tail,camera,11,133333);
    assert(short_only && short_only->count==2); // Never extend an unmatched tail.
    // Captured Skulltula: direct palette [1,10] (rigid multi-bone mode 1)
    // overlaps [1] (rigid single-bone mode 0). Both must interpolate.
    auto spider_multi=paired[0],spider_single=spider_multi;
    spider_multi.current.model=spider_single.current.model="staltula";
    spider_multi.current.bone_count=spider_single.current.bone_count=11;
    spider_multi.current.bone_ids={1,10};spider_single.current.bone_ids={1};
    spider_multi.current.skinning=1;spider_single.current.skinning=0;
    spider_single.current.key.count=1;spider_single.current.key.primitive+=0x100;
    for(bool reverse:{false,true}) {
        const auto poses=reverse?std::vector{spider_multi,spider_single}:std::vector{spider_single,spider_multi};
        const auto merged=Pose::Resolve(poses,palette,camera,11,133333);
        assert(merged && merged->count==2);
        assert(merged->palette[0]==half->palette[0]);
    }
    auto spider_bad=spider_single;spider_bad.current.skinning=2;
    assert(!Pose::Resolve({spider_multi,spider_bad},palette,camera,11,133333));
    spider_bad=spider_single;spider_bad.half[0][1][3]+=1;
    assert(!Pose::Resolve({spider_multi,spider_bad},palette,camera,11,133333));
    spider_bad=spider_single;spider_bad.current.key.skeleton+=4;
    assert(!Pose::Resolve({spider_multi,spider_bad},palette,camera,11,133333));
    spider_bad=spider_single;spider_bad.current.vertex_offset[0][3]=1;
    assert(!Pose::Resolve({spider_multi,spider_bad},palette,camera,11,133333));
    Pose::History history;auto a=paired[0].current,b=a;
    history.Observe(a);b.time+=33333;b.frame+=2;history.Observe(b);assert(!history.Pairs()[0].ready);
    b.time-=100000;history.Observe(b);assert(!history.Pairs()[0].ready); // rewind
    b.time+=33333;b.frame++;b.world[0][0][3]+=1000;history.Observe(b);assert(!history.Pairs()[0].ready);
    auto bad=Math::Identity();bad[0][1]=.5;assert(!Pose::Half(bad,bad)); // shear
    bad=Math::Identity();bad[0][0]=-1;assert(!Pose::Half(bad,bad)); // reflection
    bad=Math::Identity();bad[0][0]=NAN;assert(!Pose::Half(bad,bad));
    auto scaled=Math::Identity();scaled[0][0]=scaled[1][1]=scaled[2][2]=1.5;
    const auto scale=Pose::Half(Math::Identity(),scaled);assert(scale && (*scale)[0][0]==1.25);
    // A flattened band is still visible. Reconstruct its rotation from the
    // two surviving axes, blend it, then retain the EXACT zero thickness.
    for(unsigned axis=0;axis<3;++axis) {
        auto start=Math::Identity(),finish=Math::Identity();
        finish[0][0]=0;finish[0][1]=-1;finish[1][0]=1;finish[1][1]=0;
        finish[0][3]=20;
        auto expected=Math::Blend(start,finish,.5);
        for(unsigned row=0;row<3;++row) {
            start[row][axis]=finish[row][axis]=expected[row][axis]=0;
        }
        const auto flat=Pose::Half(start,finish);
        assert(flat && Math::Near(*flat,expected,1e-10));
        for(unsigned row=0;row<3;++row)assert((*flat)[row][axis]==0);
        assert(Math::Near(*Pose::Half(start,start),start,1e-10));
        assert(!Pose::Half(start,Math::Identity())); // collapsing/appearing
        auto invalid=start;invalid[(axis+1)%3][(axis+1)%3]=0;
        assert(!Pose::Half(invalid,invalid)); // orientation underdetermined
        invalid=start;invalid[(axis+1)%3][(axis+2)%3]=.5;
        assert(!Pose::Half(invalid,invalid)); // shear in surviving plane
        invalid=start;invalid[axis][axis]=1e-8;
        assert(!Pose::Half(invalid,invalid)); // near-zero is not exact zero
        invalid=start;invalid[(axis+1)%3][(axis+1)%3]*=3;
        assert(!Pose::Half(start,invalid)); // original scale jump guard
        invalid=start;invalid[0][3]=NAN;assert(!Pose::Half(invalid,start));
        invalid=start;invalid[3][axis]=1;assert(!Pose::Half(invalid,start));
    }
    // Recorded slingshot band, bone 23, frames 1740/1741. Its zero Y axis
    // previously rejected every adjacent pose despite an otherwise valid basis.
    const Matrix band0{{
        {-0.0007374433861931643,0,0.0022558976440245353,-373.50127439603784},
        {0.0044292492114276685,0,-0.008646337534937636,26.99138734718824},
        {0.008928570998885022,0,0.004475865227220141,-74.9805976137117},
        {0,0,0,1}}};
    const Matrix band1{{
        {-0.001178204823293069,0,0.0019795867695783977,-373.7084928762208},
        {0.00443930223248489,0,-0.008653502829401923,27.099371122879383},
        {0.008876235685153926,0,0.004591178971322687,-75.80466501212769},
        {0,0,0,1}}};
    auto band_sample=paired[0].current;band_sample.key.count=1;
    band_sample.world[0]=band0;band_sample.native[0]=Math::Multiply(band_sample.draw_view,band0);
    Pose::History band_history;band_history.Observe(band_sample);
    ++band_sample.frame;band_sample.time+=33333;
    band_sample.world[0]=band1;band_sample.native[0]=Math::Multiply(band_sample.draw_view,band1);
    band_history.Observe(band_sample);assert(band_history.Pairs()[0].ready);
    const auto band_half=Pose::Resolve(band_history.Pairs(),band_sample.native,
        band_sample.camera,band_sample.frame,band_sample.time);
    assert(band_half);
    for(unsigned row=0;row<3;++row) {
        assert(band_half->palette[0][row][1]==0);
        assert(std::abs(band_history.Pairs()[0].half[0][row][3]-(band0[row][3]+band1[row][3])/2)<1e-10);
    }
    // Regression from gameplay: the actual draw view differs from the camera
    // snapshot by .004..28 units. Valid main-camera palettes must still capture
    // and recompose in their own draw view, on both sides of the old .002 gate.
    for(double offset:{.001,.005,.14,.28}) {
        Pose::Reset();camera.time=300000;camera.view=Math::Identity();Publish(camera);guest_frames=30;
        auto draw=camera.view;draw[0][3]=offset;
        auto world=Math::Identity();world[1][3]=30;
        m.Put(stack+0x10,draw);m.Put(renderer+0x90,Math::Multiply(draw,world));
        world[1][3]=60;m.Put(renderer+0xc0,Math::Multiply(draw,world));
        Pose::Capture(m,regs,300000);
        camera.time=333333;Publish(camera);guest_frames=31;draw[0][3]=offset*1.5;
        world[0][3]=20;world[1][3]=30;
        m.Put(stack+0x10,draw);m.Put(renderer+0x90,Math::Multiply(draw,world));
        world[1][3]=60;m.Put(renderer+0xc0,Math::Multiply(draw,world));
        Pose::Capture(m,regs,333333);
        const auto captured=Pose::history.Pairs();assert(captured.size()==1 && captured[0].ready);
        const auto resolved=Pose::Resolve(captured,captured[0].current.native,camera,31,333333);
        assert(resolved && std::abs(resolved->palette[0][0][3]-(10+draw[0][3]))<.00001);
    }
    // Roundoff in the native view's basis must not move a stationary actor.
    auto draw=Math::Identity();draw[0][0]=1.0001;draw[0][3]=7000;
    auto world=Math::Identity();world[0][3]=500;
    const auto inv=Pose::InverseView(draw);assert(inv);
    const auto native_palette=Math::Multiply(draw,world);
    const auto recovered=Math::Multiply(*inv,native_palette);
    assert(Math::Near(recovered,world,.0000001));
    assert(Math::Near(Math::Multiply(draw,*Pose::Half(recovered,recovered)),native_palette,.0000001));
    // A different View owner, scene or orthographic bounds must still be rejected.
    auto count=Pose::captures.load();
    m.words[0x5be5b8+0xfc]=active_view+4;Pose::Capture(m,regs,333334);
    m.words[0x5be5b8+0xfc]=active_view;m.words[active_view+4]=320;Pose::Capture(m,regs,333334);
    m.words[active_view+4]=400;m.words[play+0x104]=3;Pose::Capture(m,regs,333334);
    m.words[play+0x104]=2;assert(Pose::captures.load()==count);
    // Malformed metadata and non-gameplay camera must never publish a pose.
    count=Pose::captures.load();m.words[primitive+0xc]=11u<<16;
    Pose::Capture(m,regs,333335);assert(Pose::captures.load()==count);
    m.words[primitive+0xc]=2u<<16;Publish({});Pose::Capture(m,regs,333335);
    assert(Pose::captures.load()==count);
    CheckLinkHierarchy(argv[1]);
    enabled=false;regs[1]=0;Pose::Capture(m,regs,133335);assert(regs[1]==renderer+0x90);
    std::cout<<"Pose replay: native guards and Link model layouts, connected joints on turns, inverse binds and vertex offsets, single-bone skinning, immutable capture/cache, flattened slingshot, native fallbacks, camera separation and existing matching regressions passed\n";
}
