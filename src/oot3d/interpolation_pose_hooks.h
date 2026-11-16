// Original USA OoT3D's dynamic skeleton-palette submission, verified in code.bin.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_pose.h"
#include "oot3d/fishing_foliage.h"
#include "oot3d/pickup_fog_hooks.h"
#include "oot3d/actor_capture.h"
#include "oot3d/title_hooks.h"
#include "oot3d/camera.h"
#include "oot3d/widescreen.h"
namespace OOT3D::Interpolation::Pose {
using TitleScreen::U32;
constexpr U32 SVC=0xfff199;
// 452914 rebuilds each visible primitive's palette on every skeleton draw.
// 2dd5d4 reads world bones at skeleton+14; 2dd628 includes inverse binds at +8
// for smooth skinning. Both apply the shape's vertex offset. 452ab0 then
// multiplies by the native view copied to sp+10. Capture after that loop so
// neither bone order nor skinning mode has to be guessed from GPU draw order.
constexpr TitleScreen::Site Site{0x452ac8,0xe2861090,0xe7903109};
inline U32 Instruction(U32 pc,U32 word) {
    return pc==Site.pc && word==Site.instruction?0xef000000|SVC:word;
}
template<class M> bool CodeMatches(M& m) {
    return m.Read(Site.pc)==Site.instruction && m.Read(Site.pc+4)==Site.following &&
        m.Read(0x452a84)==0xebfc7de6 && // copy the native view to sp+10
        m.Read(0x452ab0)==0xebfc65af && // view * each skeletal palette matrix
        m.Read(0x452ad0)==0xe1db00fe && // palette count from primitive+e
        m.Read(0x2dd5f8)==0xe5962014 && // direct world matrix array
        m.Read(0x2dd630)==0xe3520001 && // count==1 bypasses inverse bind
        m.Read(0x2dd680)==0xe5962008 && // inverse-bind array
        m.Read(0x372078)==0xee401a06;   // translate by vertex offset, not scale
}
template<class M> bool ReadMatrix(M& m,U32 address,Matrix& result) {
    std::array<float,12> values;
    if(!TitleScreen::ReadFloats(m,address,values))return false;
    result=Math::Identity();for(unsigned i=0;i<12;++i)result[i/4][i%4]=values[i];
    return true;
}
template<class M> std::shared_ptr<const Skeleton> ReadSkeleton(M& m,const Sample& sample) {
    // Verified child/adult Link CMBs have 25 40-byte bones. MM uses a
    // different stride; do not apply its layout or guess other actors' flags.
    const auto count=sample.bone_count,resource=sample.key.resource;
    if(count!=25 || m.Read(resource+4)!=16+40*count)return {};
    const auto world=m.Read(sample.key.skeleton+0x14).value_or(0);
    const auto bind=m.Read(sample.key.skeleton+8).value_or(0);
    if(!TitleScreen::Pointer(world,count*48) || !TitleScreen::Pointer(bind,count*48) ||
       !TitleScreen::Pointer(resource,16+40*count))return {};
    auto result=std::make_shared<Skeleton>();result->bones.resize(count);
    for(unsigned i=0;i<count;++i) {
        const auto info=m.Read(resource+16+40*i);
        if(!info || (*info&65535)!=i)return {};
        auto& bone=result->bones[i];bone.parent=*info>>16;
        if((i==0 && bone.parent!=65535) || (i>0 && bone.parent>=i) ||
           !ReadMatrix(m,world+48*i,bone.world) || !ReadMatrix(m,bind+48*i,bone.bind))return {};
    }
    return result;
}
// The emulation thread owns this bounded cache. Published snapshots are
// immutable and shared with replay/capture, never allocated in guest memory.
class SkeletonCache {
    struct Entry {Key key;std::uint64_t frame{};unsigned epoch{};std::int64_t time{};
        std::shared_ptr<const Skeleton> pose;};
    std::vector<Entry> entries;
public:
    template<class M> bool Attach(M& m,Sample& sample) {
        std::erase_if(entries,[&](const Entry& e) {
            return e.epoch!=sample.camera.epoch || e.frame!=sample.frame ||
                sample.time<e.time || sample.time-e.time>=10000;
        });
        auto found=std::find_if(entries.begin(),entries.end(),[&](const Entry& e) {
            return e.key.renderer==sample.key.renderer && e.key.skeleton==sample.key.skeleton &&
                e.key.resource==sample.key.resource;
        });
        if(found!=entries.end()) {
            sample.skeleton_pose=found->pose;
            if(SkeletonMatches(sample))return true;
        }
        // A changed same-frame skeleton must not reuse an earlier pose.
        sample.skeleton_pose=ReadSkeleton(m,sample);
        if(!SkeletonMatches(sample)) {sample.skeleton_pose.reset();return false;}
        if(found!=entries.end())found->pose=sample.skeleton_pose;
        else {
            if(entries.size()>=8)entries.erase(entries.begin());
            entries.push_back({sample.key,sample.frame,sample.camera.epoch,sample.time,sample.skeleton_pose});
        }
        return true;
    }
};
template<class M,class R> void Capture(M& m,R& r,std::int64_t now) {
    PickupFog::Capture(m,r,now);
    r[1]=r[6]+0x90; // Displaced ADD; no writes to live poses, palettes or animation state.
    if((!Replaying() || !enabled.load()) && !Widescreen::Active())return;
    ++capture_rejections[unsigned(CaptureStage::Calls)];
    const auto reject=[&](CaptureStage reason){
        ++capture_rejections[unsigned(reason)];
        ActorCapture::Reject(reason,r[6],r[11],guest_frames.load());
    };
    const auto camera=CameraSnapshot();
    if(!camera.valid || camera.epoch!=generation.load() || now<camera.time || now-camera.time>100000) {reject(CaptureStage::Camera);return;}
    // The View owner identifies the gameplay camera. Do not compare its matrix
    // element-by-element with the renderer's copy: the native draw view can
    // differ by tenths of a scene unit while using this same camera.
    const auto active_view=m.Read(0x5be5b8+0xfc).value_or(0);
    if(active_view!=camera.owner+0x188 || !Camera::GameplayView(m,active_view) ||
       m.Read(camera.owner+0x104)!=camera.scene) {reject(CaptureStage::Camera);return;}
    const auto renderer=r[6],primitive=r[11];
    last_renderer_type=m.Read(renderer).value_or(0);
    if(!TitleScreen::Pointer(renderer,0x480) || m.Read(renderer)!=0x4ebd98 ||
       !TitleScreen::Pointer(primitive,0x14)) {reject(CaptureStage::Renderer);return;}
    const auto skeleton=m.Read(renderer+8).value_or(0);
    if(!TitleScreen::Pointer(skeleton,0x34)) {reject(CaptureStage::Skeleton);return;}
    const auto wrapper=m.Read(skeleton+4).value_or(0);
    if(!TitleScreen::Pointer(wrapper,8)) {reject(CaptureStage::Skeleton);return;}
    const auto resource=m.Read(wrapper).value_or(0);
    last_resource=resource;
    if(!TitleScreen::Pointer(resource,16) || m.Read(resource)!=0x206c6b73) {reject(CaptureStage::Resource);return;}
    const auto bones=m.Read(resource+8).value_or(0);
    const auto count=m.Read(primitive+0xc).value_or(0)>>16;
    last_count=count;
    if(!bones || bones>128 || !count || count>10 || count>bones) {reject(CaptureStage::Count);return;}
    Matrix view;
    if(!ReadMatrix(m,r[13]+0x10,view)) {reject(CaptureStage::View);return;}
    double difference=0;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
        difference=std::max(difference,std::abs(view[row][col]-camera.view[row][col]));
    last_view_difference=float(difference);
    const auto inverse=InverseView(view);
    if(!inverse) {reject(CaptureStage::View);return;}
    Sample sample;sample.key={renderer,skeleton,resource,primitive,count};
    sample.camera=camera;sample.draw_view=view;sample.frame=guest_frames.load();sample.time=now;
    sample.bone_count=bones;sample.skinning=m.Read(primitive+0xc).value_or(0)&65535;
    const auto ids=primitive+(m.Read(primitive+0x10).value_or(0)&65535);
    for(unsigned i=0;i<count;++i) {
        const auto at=ids+2*i;
        sample.bone_ids[i]=(m.Read(at&~3u).value_or(0xffffffff)>>((at&2)*8))&65535;
    }
    // The original USA CMB header places its skeleton at +44 and model name
    // at +10. Only read the name when both header and skeleton offset agree.
    if(resource>=0x44 && m.Read(resource-0x44)==0x20626d63 &&
       m.Read(resource-0x44+0x24)==0x44) {
        for(unsigned i=0;i<16;++i) {
            const auto word=m.Read(resource-0x34+(i&~3u)).value_or(0);
            const auto c=(word>>((i&3)*8))&255;
            if(!c)break;
            sample.model+=c>=32 && c<127?char(c):'?';
        }
    }
    for(unsigned i=0;i<count;++i) {
        if(!ReadMatrix(m,renderer+0x90+48*i,sample.native[i])) {reject(CaptureStage::Matrix);return;}
        sample.world[i]=Math::Multiply(*inverse,sample.native[i]);
    }
    if(LinkModel(sample.model)) {
        std::array<float,3> offset;
        static SkeletonCache cache;
        bool valid=TitleScreen::ReadFloats(m,r[7]+0x18,offset);
        if(valid) {
            for(unsigned i=0;i<3;++i)sample.vertex_offset[i][3]=offset[i];
            valid=cache.Attach(m,sample);
        }
        if(valid)++hierarchy_captures;else ++hierarchy_rejections;
        // Still publish unsuccessful captures to invalidate older matches.
        // Link never falls back to the independent skin-matrix blend.
    }
    FishingFoliage::tags.Attach(sample);
    ActorCapture::Observe(sample);
    Publish(std::move(sample));
}
}
