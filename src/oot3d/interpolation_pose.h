// Affine pose blending adapted from DekUI's link_pose_math.h.
// Copyright 2026 MM3D presentation contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vector>
#include <string>
#include <memory>
#include "oot3d/interpolation_state.h"
#include "oot3d/song_widescreen.h"
namespace OOT3D::Interpolation::Pose {
using Palette=std::array<Matrix,10>;
inline std::atomic<bool> enabled{true};
inline std::atomic<unsigned> captures{},blended_draws{};
// Session diagnostics expose why a native pose was not used, without reading
// or changing the running guest from an external debugger.
enum class CaptureStage { Calls, Camera, Renderer, Skeleton, Resource, Count, View, Matrix, CountStages };
inline std::array<std::atomic<unsigned>,unsigned(CaptureStage::CountStages)> capture_rejections{};
inline std::atomic<unsigned> paired_palettes{},bad_cadence{},bad_transform{},duplicate_changes{};
inline std::atomic<unsigned> match_calls{},matched_palettes{},unpaired_matches{},ambiguous_matches{};
inline std::atomic<unsigned> shared_prefix_matches{};
inline std::atomic<unsigned> hierarchy_captures{},hierarchy_rejections{},hierarchy_pairs{};
inline std::atomic<std::uint32_t> last_renderer_type{},last_resource{},last_count{};
inline std::atomic<float> last_view_difference{};
// Share the exact affine inverse with camera matching and preview history.
inline std::optional<Matrix> InverseView(const Matrix& m) {return Math::InverseCamera(m);}
inline std::optional<Matrix> Half(const Matrix& a,const Matrix& b,double fraction=.5) {
    Matrix ar=a,br=b;std::array<double,3> scales{};
    std::optional<unsigned> flat_axis;
    for(unsigned col=0;col<3;++col) {
        double as=0,bs=0;
        for(unsigned row=0;row<3;++row) {as+=a[row][col]*a[row][col];bs+=b[row][col]*b[row][col];}
        as=std::sqrt(as);bs=std::sqrt(bs);
        // Some visible meshes (including Link's slingshot band) flatten one
        // axis to exactly zero. Two surviving axes still define orientation.
        // Only accept the same collapsed axis in both samples; visibility
        // changes, nearly singular transforms and multiple lost axes stay native.
        if(as==0 && bs==0) {
            if(flat_axis)return {};
            flat_axis=col;scales[col]=0;continue;
        }
        if(!std::isfinite(as) || !std::isfinite(bs) || as<1e-5 || bs<1e-5 || as/bs<.5 || as/bs>2)return {};
        scales[col]=std::lerp(as,bs,fraction);
        for(unsigned row=0;row<3;++row) {ar[row][col]/=as;br[row][col]/=bs;}
    }
    if(flat_axis) {
        const unsigned col=*flat_axis,j=(col+1)%3,k=(col+2)%3;
        for(Matrix* rotation:{&ar,&br})for(unsigned row=0;row<3;++row) {
            const unsigned next=(row+1)%3,last=(row+2)%3;
            (*rotation)[row][col]=(*rotation)[next][j]*(*rotation)[last][k]-
                                  (*rotation)[last][j]*(*rotation)[next][k];
        }
    }
    // Reflections, shear and sudden scale changes keep the native pose.
    if(!Math::Rigid(ar) || !Math::Rigid(br))return {};
    auto result=Math::Blend(ar,br,fraction);
    for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)result[row][col]*=scales[col];
    return result;
}
// Adult Link's bowstring deliberately collapses and reverses its local Y
// scale during recoil. Recover orientation from its two stable axes, then
// interpolate the signed stretch; a rigid/positive-scale test rejects it.
inline std::optional<Matrix> HalfBowString(const Matrix& a,const Matrix& b,double fraction=.5) {
    Matrix ar=a,br=b;std::array<double,3> scales{};
    for(unsigned col:{0u,2u}) {
        double as=0,bs=0;
        for(unsigned row=0;row<3;++row) {as+=a[row][col]*a[row][col];bs+=b[row][col]*b[row][col];}
        as=std::sqrt(as);bs=std::sqrt(bs);
        if(!std::isfinite(as) || !std::isfinite(bs) || as<1e-5 || bs<1e-5 || as/bs<.5 || as/bs>2)return {};
        scales[col]=std::lerp(as,bs,fraction);
        for(unsigned row=0;row<3;++row) {ar[row][col]/=as;br[row][col]/=bs;}
    }
    std::array<double,2> stretch{};
    for(unsigned sample=0;sample<2;++sample) {
        auto& rotation=sample?br:ar;const auto& original=sample?b:a;
        for(unsigned row=0;row<3;++row) {
            const unsigned next=(row+1)%3,last=(row+2)%3;
            rotation[row][1]=rotation[next][2]*rotation[last][0]-rotation[last][2]*rotation[next][0];
            stretch[sample]+=original[row][1]*rotation[row][1];
        }
        if(!Math::Rigid(rotation) || !std::isfinite(stretch[sample]) || std::abs(stretch[sample])>4)return {};
        for(unsigned row=0;row<3;++row)
            if(!std::isfinite(original[row][1]) ||
               std::abs(original[row][1]-rotation[row][1]*stretch[sample])>.002)return {};
    }
    scales[1]=std::lerp(stretch[0],stretch[1],fraction);
    auto result=Math::Blend(ar,br,fraction);
    for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)result[row][col]*=scales[col];
    return result;
}
// Child Link's slingshot band also shears during release: its Y axis is
// not perpendicular to X/Z. Preserve that elastic deformation with an affine
// midpoint in the parent's frame, rather than forcing a rigid decomposition.
// This is restricted below to the verified band bone, never body joints.
inline std::optional<Matrix> HalfSlingshotString(const Matrix& a,const Matrix& b,double fraction=.5) {
    if(!Math::Near(a,a,0) || !Math::Near(b,b,0))return {};
    for(const auto* m:{&a,&b}) {
        if((*m)[3]!=std::array<double,4>{0,0,0,1})return {};
        double xx=0,zz=0,xz=0;
        for(unsigned row=0;row<3;++row) {
            xx+=(*m)[row][0]*(*m)[row][0];zz+=(*m)[row][2]*(*m)[row][2];
            xz+=(*m)[row][0]*(*m)[row][2];
        }
        if(xx*zz-xz*xz<.25*xx*zz)return {}; // Support axes must remain independent.
        for(unsigned col=0;col<3;++col) {
            double length=0;
            for(unsigned row=0;row<3;++row)length+=(*m)[row][col]*(*m)[row][col];
            // X/Z survive recoil at unit local scale; Y collapses, reverses
            // and stretches. Reject lost support axes and oversized transforms.
            if(length>16 || (col!=1 && length<.25))return {};
        }
    }
    for(unsigned col:{0u,2u}) {
        double aa=0,bb=0,ab=0;
        for(unsigned row=0;row<3;++row) {
            aa+=a[row][col]*a[row][col];bb+=b[row][col]*b[row][col];
            ab+=a[row][col]*b[row][col];
        }
        if(aa/bb<.25 || aa/bb>4 || ab/std::sqrt(aa*bb)<.5)return {};
    }
    auto result=Math::Identity();
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
        result[row][col]=std::lerp(a[row][col],b[row][col],fraction);
    return result;
}
inline bool PaletteNear(const Matrix& a,const Matrix& b) {
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        if(!std::isfinite(a[r][c]) || !std::isfinite(b[r][c]) ||
           std::abs(a[r][c]-b[r][c])>(c==3?.03:1e-5))return false;
    return true;
}
// Like DekUI, interpolate joints in their parents' frames, BEFORE applying
// inverse binds. Independent world/skin matrix interpolation changes bone
// lengths on turns and pulls the vertices between connected joints apart.
struct Skeleton {
    struct Bone {unsigned parent{65535};Matrix world{},bind{};};
    std::vector<Bone> bones;
};
using SkeletonHalf=std::vector<std::optional<Matrix>>;
inline bool HierarchyModel(const std::string& name) {
    return name=="childlink_v2" || name=="link_v2" || name=="rb_cap";
}
inline std::optional<Matrix> InverseBone(const Matrix& m) {
    if(!Math::Near(m,m,0))return {};
    for(unsigned i=0;i<4;++i)if(std::abs(m[3][i]-(i==3?1.:0.))>1e-6)return {};
    const auto a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],
        g=m[2][0],h=m[2][1],i=m[2][2];
    const auto det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det) || std::abs(det)<1e-12)return {};
    auto r=Math::Identity();
    r[0]={(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,0};
    r[1]={(f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,0};
    r[2]={(d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det,0};
    for(unsigned row=0;row<3;++row)for(unsigned k=0;k<3;++k)r[row][3]-=r[row][k]*m[k][3];
    return Math::Near(r,r,0)?std::optional{r}:std::nullopt;
}
inline SkeletonHalf BlendSkeleton(const Skeleton& a,const Skeleton& b,bool adult_link=false,bool child_link=false,double fraction=.5) {
    const auto count=b.bones.size();
    if(!count || count>128 || a.bones.size()!=count)return {};
    SkeletonHalf half(count);
    std::vector<std::optional<Matrix>> ai(count),bi(count);
    const double limit=distance_limit.load();
    if(!std::isfinite(limit) || limit<=0)return {};
    for(unsigned i=0;i<count;++i) {
        const auto& old=a.bones[i];const auto& next=b.bones[i];
        if(old.parent!=next.parent || (next.parent!=65535 && next.parent>=i) ||
           old.bind!=next.bind)return {};
        ai[i]=InverseBone(old.world);bi[i]=InverseBone(next.world);
    }
    for(unsigned i=0;i<count;++i) {
        const auto& old=a.bones[i];const auto& next=b.bones[i];
        auto local_a=old.world,local_b=next.world,parent_half=Math::Identity();
        if(next.parent==65535) {
            double distance=0;
            for(unsigned axis=0;axis<3;++axis)distance+=std::pow(local_b[axis][3]-local_a[axis][3],2);
            if(!std::isfinite(distance) || distance>limit*limit)return {};
        } else {
            const auto p=next.parent;
            if(!ai[p] || !bi[p] || !half[p])continue;
            local_a=Math::Multiply(*ai[p],local_a);local_b=Math::Multiply(*bi[p],local_b);
            parent_half=*half[p];
        }
        const bool bow_string=adult_link && count==25 && i==23 && next.parent==22 &&
            b.bones[22].parent==0;
        const bool slingshot_string=child_link && count==25 && i==23 && next.parent==22 &&
            b.bones[22].parent==0;
        const auto local=bow_string?HalfBowString(local_a,local_b,fraction):
            slingshot_string?HalfSlingshotString(local_a,local_b,fraction):Half(local_a,local_b,fraction);
        // Unsupported branches stay native. A hidden/collapsed weapon must
        // not prevent the rest of Link's body from interpolating.
        if(local)half[i]=Math::Multiply(parent_half,*local);
    }
    return half;
}
struct Key {
    std::uint32_t renderer{},skeleton{},resource{},primitive{};
    unsigned count{};
    bool operator==(const Key&) const=default;
};
struct Sample {
    Key key;
    Palette native{},world{};
    CameraSample camera;
    Matrix draw_view=Math::Identity();
    std::uint64_t frame{};
    std::int64_t time{};
    // The name gates the verified Link hierarchy path. Bone IDs and skinning
    // mode establish whether different palettes share the same joints.
    std::string model;
    std::array<unsigned,10> bone_ids{};
    unsigned skinning{},bone_count{};
    // Fishing prop renderers are pooled by visibility, not by plant identity.
    std::uint32_t instance_owner{},instance{};
    // Immutable host-only snapshot shared by all meshes of this skeleton.
    std::shared_ptr<const Skeleton> skeleton_pose;
    Matrix vertex_offset=Math::Identity();
    unsigned shop_slot=8;
    std::int64_t shop_press_us=0;
};
inline bool FishingPlant(const Sample& s) {
    return (s.model=="turi_kusa_model" || s.model=="turi_hasu_model") &&
        (s.camera.scene&65535)==73 && s.bone_count==1 && s.skinning==0 &&
        s.key.count==1 && s.bone_ids[0]==0;
}
inline bool SameHistory(const Sample& a,const Sample& b) {
    if(FishingPlant(a) || FishingPlant(b)) {
        if(!FishingPlant(a) || !FishingPlant(b) || a.model!=b.model)return false;
        if(a.instance && b.instance)
            return a.instance_owner==b.instance_owner && a.instance==b.instance &&
                a.key.resource==b.key.resource && a.key.primitive==b.key.primitive;
        // Retain untagged samples for native-palette ambiguity checks, but
        // never pair them across frames using a recycled renderer slot.
        return !a.instance && !b.instance && a.key==b.key;
    }
    return a.key==b.key;
}
inline bool GohmaEgg(const Sample& s) {
    // childgoma's intact egg is the single rigid mesh on bone 11. Its other
    // palettes belong to the hatched spider and retain normal pose blending.
    return s.model=="childgoma" && s.bone_count==12 && s.skinning==0 &&
        s.key.count==1 && s.bone_ids[0]==11;
}
inline std::optional<Matrix> HalfElasticPalette(const Matrix& a,const Matrix& b,
        double fraction,double max_shear,double min_volume=.8) {
    // Animated nonuniform scale can precede rotation, producing deliberate
    // shear. Blend that small elastic deformation intact.
    // Guard the basis to exclude collapsed hatching poses, flips and large turns.
    if(!Math::Near(a,a,0) || !Math::Near(b,b,0))return {};
    std::array<Matrix,2> normalized{a,b};
    std::array<std::array<double,3>,2> scales{};
    for(unsigned sample=0;sample<2;++sample) {
        auto& n=normalized[sample];
        if(n[3]!=std::array<double,4>{0,0,0,1})return {};
        for(unsigned c=0;c<3;++c) {
            double length=0;for(unsigned r=0;r<3;++r)length+=n[r][c]*n[r][c];
            const double scale=std::sqrt(length);
            if(scale<1e-5 || scale>1)return {};
            scales[sample][c]=scale;
            for(unsigned r=0;r<3;++r)n[r][c]/=scale;
        }
        for(unsigned i=0;i<3;++i)for(unsigned j=i+1;j<3;++j) {
            double dot=0;for(unsigned r=0;r<3;++r)dot+=n[r][i]*n[r][j];
            if(std::abs(dot)>max_shear)return {};
        }
        const double det=n[0][0]*(n[1][1]*n[2][2]-n[1][2]*n[2][1])-
            n[0][1]*(n[1][0]*n[2][2]-n[1][2]*n[2][0])+
            n[0][2]*(n[1][0]*n[2][1]-n[1][1]*n[2][0]);
        if(det<min_volume)return {};
    }
    for(unsigned c=0;c<3;++c) {
        const double ratio=scales[0][c]/scales[1][c];
        double dot=0;for(unsigned r=0;r<3;++r)dot+=normalized[0][r][c]*normalized[1][r][c];
        if(ratio<.5 || ratio>2 || dot<.8)return {};
    }
    auto result=Math::Identity();
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        result[r][c]=std::lerp(a[r][c],b[r][c],fraction);
    return result;
}
inline std::optional<Matrix> HalfGohmaEgg(const Matrix& a,const Matrix& b,double fraction=.5) {
    return HalfElasticPalette(a,b,fraction,.35);
}
inline bool BariSkin(const Sample& s) {
    if(s.model!="vali" || s.bone_count!=11 || s.skinning!=2)return false;
    constexpr std::array<unsigned,9> outer{7,8,9,6,0,2,3,4,1};
    constexpr std::array<unsigned,8> inner{7,8,9,6,2,3,4,1};
    return (s.key.count==outer.size() && std::equal(outer.begin(),outer.end(),s.bone_ids.begin())) ||
        (s.key.count==inner.size() && std::equal(inner.begin(),inner.end(),s.bone_ids.begin()));
}
inline bool TorchSlugSkin(const Sample& s) {
    constexpr std::array<unsigned,5> body{0,1,2,3,4};
    return s.model=="torchsrag" && s.bone_count==5 && s.skinning==2 &&
        s.key.count==body.size() && std::equal(body.begin(),body.end(),s.bone_ids.begin());
}
inline bool CowSkin(const Sample& s) {
    if((s.model!="cow" && s.model!="cow2") || s.bone_count!=10 || s.skinning!=2)return false;
    constexpr std::array<unsigned,10> body{0,6,5,7,8,9,2,4,1,3};
    return (s.key.count==body.size() && s.bone_ids==body) ||
           (s.key.count==2 && s.bone_ids[0]==1 && s.bone_ids[1]==3);
}
inline bool PeahatBody(const Sample& s) {
    if(s.model!="peehat" || s.bone_count!=10 || s.bone_ids[0]!=1)return false;
    if(s.skinning==0)return s.key.count==1;
    if(s.skinning==2)return s.key.count==2 && s.bone_ids[1]==9;
    constexpr std::array<unsigned,4> left{1,8,4,6},right{1,5,7,3};
    return s.skinning==1 && s.key.count==4 &&
        (std::equal(left.begin(),left.end(),s.bone_ids.begin()) ||
         std::equal(right.begin(),right.end(),s.bone_ids.begin()));
}
inline bool WastelandFlagCloth(const Sample& s) {
    if(s.model!="ht_hata" || s.bone_count!=20 || s.skinning!=2 || s.key.count!=4)return false;
    constexpr std::array<unsigned,4> first{15,17,13,19},second{8,10,6,4};
    return std::equal(first.begin(),first.end(),s.bone_ids.begin()) ||
           std::equal(second.begin(),second.end(),s.bone_ids.begin());
}
inline bool LikeLikeBody(const Sample& s) {
    constexpr std::array<unsigned,5> body{3,4,2,1,0};
    return s.model=="likelike" && s.bone_count==5 && s.skinning==1 &&
        s.key.count==body.size() && std::equal(body.begin(),body.end(),s.bone_ids.begin());
}
inline bool MorphaCore(const Sample& s) {
    return (s.model=="co_in_modelT" || s.model=="co_out_modelT") &&
        s.bone_count==1 && s.skinning==0 && s.key.count==1 && s.bone_ids[0]==0;
}
inline std::optional<Matrix> HalfMorphaCore(const Matrix& a,const Matrix& b,double fraction=.5) {
    // The core spins too quickly for the slow elastic affine blend. Separate
    // rotation from stretch with polar decomposition so its wobble survives
    // without shrinking the rotating ball between native frames.
    std::array<Matrix,2> rotations{a,b},stretch{Math::Identity(),Math::Identity()};
    std::array<std::array<double,3>,2> scales{};
    for(unsigned sample=0;sample<2;++sample) {
        const auto& native=sample?b:a;
        // Reuse finite, positive-volume, bounded shear and scale validation.
        // Captured core shear reaches .402; other actors keep their own limits.
        if(!HalfElasticPalette(native,native,.5,.45))return {};
        double largest=0;
        for(unsigned c=0;c<3;++c) {
            double length=0;for(unsigned r=0;r<3;++r)length+=native[r][c]*native[r][c];
            scales[sample][c]=std::sqrt(length);largest=std::max(largest,scales[sample][c]);
        }
        auto& rotation=rotations[sample];
        for(unsigned r=0;r<3;++r) {
            rotation[r][3]=0;
            for(unsigned c=0;c<3;++c)rotation[r][c]/=largest;
        }
        for(unsigned iteration=0;iteration<16;++iteration) {
            const auto inverse=InverseBone(rotation);if(!inverse)return {};
            auto next=rotation;double delta=0;
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c) {
                next[r][c]=.5*(rotation[r][c]+(*inverse)[c][r]);
                delta=std::max(delta,std::abs(next[r][c]-rotation[r][c]));
            }
            rotation=next;if(delta<1e-10)break;
        }
        if(!Math::Rigid(rotation))return {};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c) {
            stretch[sample][r][c]=0;
            for(unsigned k=0;k<3;++k)stretch[sample][r][c]+=rotation[k][r]*native[k][c];
        }
        for(unsigned r=0;r<3;++r)rotation[r][3]=native[r][3];
    }
    for(unsigned c=0;c<3;++c) {
        const auto ratio=scales[0][c]/scales[1][c];
        if(ratio<.5 || ratio>2)return {};
    }
    const auto qa=Math::Rotation(rotations[0]),qb=Math::Rotation(rotations[1]);
    double dot=0;for(unsigned i=0;i<4;++i)dot+=qa[i]*qb[i];
    if(std::abs(dot)<std::sqrt(.5))return {}; // Reject turns over 90 degrees per tick.
    auto shape=Math::Identity();
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        shape[r][c]=std::lerp(stretch[0][r][c],stretch[1][r][c],fraction);
    return Math::Multiply(Math::Blend(rotations[0],rotations[1],fraction),shape);
}
inline bool ShopSelectionChanged(const Sample& a,const Sample& b) {
    return a.shop_slot<8 && b.shop_slot<8 && a.shop_slot!=b.shop_slot;
}
// Stalfos removes individual pieces by zeroing their entire world transform.
inline bool HiddenStalfosBone(const Sample& s,unsigned slot) {
    const auto& m=s.native[slot];
    for(unsigned r=0;r<3;++r) {
        if(m[r][0]!=0 || m[r][1]!=0 || m[r][2]!=0 || m[r][3]!=s.draw_view[r][3])return false;
    }
    return s.model=="stalfos" && s.bone_count==25;
}
inline bool StalfosVisibilityChange(const Sample& a,const Sample& b,unsigned slot) {
    return (HiddenStalfosBone(a,slot) || HiddenStalfosBone(b,slot)) &&
        a.model==b.model && a.bone_count==b.bone_count && a.skinning==b.skinning &&
        a.bone_ids==b.bone_ids;
}
inline std::optional<Matrix> BlendPalette(const Sample& a,const Sample& b,unsigned slot,double fraction=.5) {
    if(StalfosVisibilityChange(a,b,slot)) {
        // Snap visibility at the native tick. Blending to zero would drag a
        // disappearing limb to the world origin and stretch its triangles.
        return fraction<1?a.world[slot]:b.world[slot];
    }
    if(ShopSelectionChanged(a,b)) {
        // Sample the discrete selection at the HID edge. Native repeats with
        // no fresh press use the current item. Never blend across the shelf.
        const auto displacement=std::llround(double(b.time-a.time)*fraction);
        return a.time+displacement<b.shop_press_us?a.world[slot]:b.world[slot];
    }
    if(MorphaCore(a) && MorphaCore(b) && a.model==b.model) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        return HalfMorphaCore(a.world[slot],b.world[slot],fraction);
    }
    if(CowSkin(a) && CowSkin(b) && a.model==b.model && a.key.count==b.key.count && a.bone_ids==b.bone_ids) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        // The swinging tail reaches .665 normalized shear and .722 normalized
        // volume at the tip. One rejected joint drops the entire body draw.
        return HalfElasticPalette(a.world[slot],b.world[slot],fraction,.70,.70);
    }
    // Adults reach .145 shear, while the flying larvae reach .472 on the same rig.
    // Keep the root identical across the blade and body material passes.
    if(PeahatBody(a) && PeahatBody(b) && a.skinning==b.skinning && a.bone_ids==b.bone_ids) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        return HalfElasticPalette(a.world[slot],b.world[slot],fraction,.5);
    }
    // Captured normalized shear reaches .074 on Like Like and .095 on the flag cloth.
    if((LikeLikeBody(a) && LikeLikeBody(b)) ||
       (WastelandFlagCloth(a) && WastelandFlagCloth(b) && a.bone_ids==b.bone_ids)) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        return HalfElasticPalette(a.world[slot],b.world[slot],fraction,.1);
    }
    // Torch Slug squash precedes joint rotation: its verified five-bone body
    // reaches .1784 normalized shear. Preserve this mild deformation without
    // relaxing rigid checks for its other mesh or unrelated actors.
    if(TorchSlugSkin(a) && TorchSlugSkin(b)) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        return HalfElasticPalette(a.world[slot],b.world[slot],fraction,.2);
    }
    if(GohmaEgg(a) && GohmaEgg(b))return HalfGohmaEgg(a.world[slot],b.world[slot],fraction);
    // Bari's pulsating bell stretches before skinning, leaving mild shear in
    // the two smooth palettes (9 and 8 joints). Retain the rigid blend where
    // possible, and preserve the verified elastic deformation otherwise.
    // Bones 4/9 reach .463 normalized shear during the captured spin; the
    // egg's .35 limit rejected the whole palette. Only these two known Bari
    // layouts use .5; determinant, scale and inter-frame turn guards still apply.
    if(BariSkin(a) && BariSkin(b) && a.bone_ids==b.bone_ids && a.key.count==b.key.count) {
        if(const auto rigid=Half(a.world[slot],b.world[slot],fraction))return rigid;
        return HalfElasticPalette(a.world[slot],b.world[slot],fraction,.5);
    }
    return Half(a.world[slot],b.world[slot],fraction);
}
inline std::optional<Matrix> SkinBone(const Sample& sample,unsigned slot,const Matrix& world) {
    if(!sample.skeleton_pose || slot>=sample.key.count)return {};
    const auto id=sample.bone_ids[slot];
    if(id>=sample.skeleton_pose->bones.size())return {};
    // The native smooth builder deliberately skips the bind for count==1.
    const auto offset=sample.skinning==2 && sample.key.count>1?
        Math::Multiply(sample.skeleton_pose->bones[id].bind,sample.vertex_offset):sample.vertex_offset;
    return Math::Multiply(world,offset);
}
inline bool SkeletonMatches(const Sample& sample) {
    if(!sample.skeleton_pose || sample.skeleton_pose->bones.size()!=sample.bone_count)return false;
    for(unsigned i=0;i<sample.key.count;++i) {
        const auto id=sample.bone_ids[i];
        if(id>=sample.bone_count)return false;
        const auto skin=SkinBone(sample,i,sample.skeleton_pose->bones[id].world);
        if(!skin || !PaletteNear(Math::Multiply(sample.draw_view,*skin),sample.native[i]))return false;
    }
    return true;
}
struct Pair {Sample current;Palette half{};bool ready{};Sample previous;};
struct MatchDiagnostic {
    Key first{},second{};
    std::string description;
    unsigned hits{};
};
inline std::mutex diagnostic_mutex;
inline std::vector<MatchDiagnostic> match_diagnostics;
inline std::string Describe(const Sample& s) {
    auto text=s.model+" bones="+std::to_string(s.bone_count)+" skin="+
        std::to_string(s.skinning)+" palette=[";
    for(unsigned i=0;i<s.key.count;++i)text+=(i?",":"")+std::to_string(s.bone_ids[i]);
    return text+"]";
}
inline void Diagnose(const Pair& first,const Pair* second) {
    std::scoped_lock lock{diagnostic_mutex};
    const Key other=second?second->current.key:Key{};
    for(auto& d:match_diagnostics)if(d.first==first.current.key && d.second==other) {
        ++d.hits;return;
    }
    if(match_diagnostics.size()>=32)return;
    auto description=Describe(first.current);
    if(second)description+=" conflicts with "+Describe(second->current);
    else description+=" has no adjacent pose";
    match_diagnostics.push_back({first.current.key,other,std::move(description),1});
}
inline std::vector<MatchDiagnostic> TakeDiagnostics() {
    std::scoped_lock lock{diagnostic_mutex};
    auto result=std::move(match_diagnostics);match_diagnostics.clear();return result;
}
class History {
    std::vector<Pair> live;
    struct BlendedSkeleton {
        std::shared_ptr<const Skeleton> previous,current;
        SkeletonHalf half;
    };
    std::vector<BlendedSkeleton> skeletons;
    const SkeletonHalf& Midpoint(const Sample& previous,const Sample& next) {
        for(const auto& cached:skeletons)
            if(cached.previous==previous.skeleton_pose && cached.current==next.skeleton_pose)return cached.half;
        if(skeletons.size()>=16)skeletons.erase(skeletons.begin());
        skeletons.push_back({previous.skeleton_pose,next.skeleton_pose,
            BlendSkeleton(*previous.skeleton_pose,*next.skeleton_pose,next.model=="link_v2",next.model=="childlink_v2")});
        return skeletons.back().half;
    }
public:
    void Reset() {live.clear();skeletons.clear();}
    const std::vector<Pair>& Pairs() const {return live;}
    void Observe(Sample next) {
        if(!next.key.count || next.key.count>10 || !next.camera.valid)return;
        std::erase_if(live,[&](const auto& p) {
            return p.current.camera.epoch!=next.camera.epoch || p.current.time>next.time ||
                next.time-p.current.time>100000 || p.current.camera.owner!=next.camera.owner ||
                p.current.camera.scene!=next.camera.scene;
        });
        auto found=std::find_if(live.begin(),live.end(),[&](const auto& p){return SameHistory(p.current,next);});
        if(found==live.end()) {
            if(live.size()>=512)live.erase(live.begin());
            live.push_back({std::move(next),{},false});return;
        }
        const auto& previous=found->current;
        // Opaque/translucent passes can rebuild the same palette in one frame.
        // A changed pose in that interval is ambiguous; never pair it with itself.
        if(next.frame==previous.frame && next.time-previous.time<10000) {
            bool same=next.camera.view==previous.camera.view && next.draw_view==previous.draw_view &&
                next.skeleton_pose==previous.skeleton_pose && next.bone_ids==previous.bone_ids &&
                next.skinning==previous.skinning && next.vertex_offset==previous.vertex_offset;
            for(unsigned i=0;i<next.key.count;++i)same &= next.native[i]==previous.native[i];
            if(same)return;
            ++duplicate_changes;
            *found={std::move(next),{},false};return;
        }
        const auto elapsed=next.time-previous.time;
        Pair pair;pair.ready=elapsed>=20000 && elapsed<=50000 && next.frame==previous.frame+1;
        if(FishingPlant(next) && (!next.instance || !next.instance_owner))pair.ready=false;
        if(!pair.ready)++bad_cadence;
        if(pair.ready && (HierarchyModel(previous.model) || HierarchyModel(next.model))) {
            pair.ready=previous.model==next.model && previous.skeleton_pose && next.skeleton_pose &&
                previous.bone_ids==next.bone_ids && previous.skinning==next.skinning &&
                previous.vertex_offset==next.vertex_offset;
            if(pair.ready) {
                const auto& half=Midpoint(previous,next);
                for(unsigned i=0;i<next.key.count && pair.ready;++i) {
                    const auto id=next.bone_ids[i];
                    const auto skin=id<half.size() && half[id]?SkinBone(next,i,*half[id]):std::nullopt;
                    pair.ready=bool(skin);
                    if(skin)pair.half[i]=*skin;
                }
            }
            if(pair.ready)++hierarchy_pairs;else ++bad_transform;
        } else for(unsigned i=0;i<next.key.count && pair.ready;++i) {
            double distance=0;
            for(unsigned axis=0;axis<3;++axis)distance+=std::pow(next.world[i][axis][3]-previous.world[i][axis][3],2);
            const auto limit=distance_limit.load();
            const auto half=BlendPalette(previous,next,i);
            pair.ready=half && std::isfinite(distance) &&
                ((std::isfinite(limit) && distance<=double(limit)*limit) ||
                 ShopSelectionChanged(previous,next) || StalfosVisibilityChange(previous,next,i));
            if(!pair.ready)++bad_transform;
            if(pair.ready)pair.half[i]=*half;
        }
        if(pair.ready)++paired_palettes;
        pair.previous=previous;pair.current=std::move(next);*found=std::move(pair);
    }
};
inline std::mutex mutex;
inline History history;
inline std::atomic<std::uint64_t> revision{};
inline void Publish(Sample sample) {
    std::scoped_lock lock{mutex};history.Observe(std::move(sample));++revision;++captures;
}
inline void Reset() {std::scoped_lock lock{mutex};history.Reset();++revision;}
struct Override {Palette palette;unsigned count{};};
inline bool SamePalettePrefix(const Sample& a,const Sample& b) {
    // Native rigid modes 0 (single bone) and 1 (multi-bone) both use direct
    // world matrices. Skulltula draws [1,10] then [1]; their shared bone is
    // identical despite different modes. Mode 2 includes inverse binds and
    // must still agree exactly. Resolve also compares every blended overlap.
    const bool same_skinning=a.skinning==b.skinning ||
        (a.skinning<=1 && b.skinning<=1 && a.model==b.model &&
         a.vertex_offset==b.vertex_offset);
    if(a.key.renderer!=b.key.renderer || a.key.skeleton!=b.key.skeleton ||
       a.key.resource!=b.key.resource || a.frame!=b.frame ||
       !same_skinning || !a.bone_count || a.bone_count!=b.bone_count)return false;
    for(unsigned i=0;i<std::min(a.key.count,b.key.count);++i)
        if(a.bone_ids[i]>=a.bone_count || a.bone_ids[i]!=b.bone_ids[i])return false;
    return true;
}
inline bool SkinnedRootPrefix(const Pair& body,const Pair& root) {
    // Torch Slug and Peahat each draw a single rigid joint also used by their
    // skinned mesh. The short upload leaves the other palette registers intact.
    // Require both endpoints and ownership to agree before accepting overlap.
    const auto layout=[](const Sample& skin,const Sample& rigid) {
        const bool shared_root=(TorchSlugSkin(skin) && rigid.model=="torchsrag" &&
            rigid.bone_count==5 && rigid.bone_ids[0]==0) ||
            (PeahatBody(skin) && skin.skinning==2 && PeahatBody(rigid));
        return shared_root && rigid.skinning==0 && rigid.key.count==1 &&
            skin.key.renderer==rigid.key.renderer && skin.key.skeleton==rigid.key.skeleton &&
            skin.key.resource==rigid.key.resource && skin.frame==rigid.frame &&
            skin.vertex_offset==rigid.vertex_offset && PaletteNear(skin.world[0],rigid.world[0]);
    };
    return layout(body.previous,root.previous) && layout(body.current,root.current);
}
// In the castle-escape cutscene the two-joint mesh starts with joint 8,
// which can coincide with rigid joint 11. Require both sampled endpoints to
// agree, since the two joints can move independently in another animation.
inline bool GanondorfIntroPrefix(const Pair& mesh,const Pair& rigid) {
    const auto shared=[](const Sample& a,const Sample& b) {
        return a.model=="ganondorfchild" && b.model==a.model && a.bone_count==22 && b.bone_count==22 &&
            a.skinning==1 && a.key.count==2 && a.bone_ids[0]==8 && a.bone_ids[1]==1 &&
            b.skinning==0 && b.key.count==1 && b.bone_ids[0]==11 &&
            a.key.renderer==b.key.renderer && a.key.skeleton==b.key.skeleton && a.key.resource==b.key.resource &&
            a.frame==b.frame && a.vertex_offset==b.vertex_offset && PaletteNear(a.world[0],b.world[0]);
    };
    return shared(mesh.previous,rigid.previous) && shared(mesh.current,rigid.current);
}
inline bool GraveyardMaskPrefix(const Pair& actor,const Pair& mask) {
    // The graveyard boy's Spooky Mask is a separate renderer attached to
    // head bone 14. Its one matrix also matches the [14,7] head/body draw.
    // Require the verified layouts AND agreement at both native endpoints;
    // a coincidentally aligned actor or a newly attached mask is not enough.
    const auto layout=[](const Sample& boy,const Sample& accessory) {
        return boy.model=="childstalker" && boy.bone_count==15 && boy.skinning==1 &&
            boy.key.count==2 && boy.bone_ids[0]==14 && boy.bone_ids[1]==7 &&
            accessory.model=="redead__mask" && accessory.bone_count==1 &&
            accessory.skinning==0 && accessory.key.count==1 && accessory.bone_ids[0]==0 &&
            boy.frame==accessory.frame && boy.vertex_offset==accessory.vertex_offset &&
            PaletteNear(boy.world[0],accessory.world[0]);
    };
    return layout(actor.previous,mask.previous) && layout(actor.current,mask.current);
}
// Meshes of the same actor share immutable source skeletons. Reconstruct each
// hierarchy once per phase, not once for every palette/mesh that references it.
class PhaseSkeletonCache {
    struct Entry {
        std::shared_ptr<const Skeleton> previous,current;
        std::string model;
        float fraction{};
        SkeletonHalf bones;
    };
    std::vector<Entry> entries;
public:
    unsigned builds{},hits{};
    const SkeletonHalf& Get(const Sample& previous,const Sample& current,float fraction) {
        for(const auto& entry:entries)if(entry.previous==previous.skeleton_pose &&
            entry.current==current.skeleton_pose && entry.model==current.model && entry.fraction==fraction) {
            ++hits;return entry.bones;
        }
        if(entries.size()>=16)entries.erase(entries.begin());
        ++builds;
        entries.push_back({previous.skeleton_pose,current.skeleton_pose,current.model,fraction,
            BlendSkeleton(*previous.skeleton_pose,*current.skeleton_pose,
                current.model=="link_v2",current.model=="childlink_v2",fraction)});
        return entries.back().bones;
    }
};
// Native palette values establish ownership; draw order and vertex count do not.
// Equivalent-looking actors with different intermediate poses are ambiguous.
inline std::optional<Override> Resolve(const std::vector<Pair>& pairs,const Palette& native,
        const CameraSample& camera,std::uint64_t frame,std::int64_t now,PhaseSkeletonCache* cache=nullptr,unsigned vertices=0) {
    std::optional<Override> result;
    const Pair* selected=nullptr;
    ++match_calls;
    for(const auto& p:pairs) {
        const auto& s=p.current;
        if(s.camera.epoch!=camera.epoch || s.camera.owner!=camera.owner || s.camera.scene!=camera.scene ||
           now<s.time || now-s.time>100000 || frame<s.frame || frame-s.frame>1 ||
           !Math::Near(s.camera.view,camera.view,.002))continue;
        bool match=true;
        for(unsigned i=0;i<s.key.count && match;++i)match=PaletteNear(native[i],s.native[i]);
        if(!match)continue;
        // At rest the face can match the first two body matrices. Both cow
        // CMBs submit 126 indices for the face and 2274 for the body/tail.
        if(vertices && CowSkin(s) && vertices!=(s.key.count==10?2274u:126u))continue;
        // A hidden rigid piece otherwise matches any skin whose first bone
        // has disappeared. Disambiguate the captured Stalfos accessory draws.
        if(vertices && s.key.count==1 && HiddenStalfosBone(s,0)) {
            const auto bone=s.bone_ids[0];
            if((bone==5 && vertices!=24) || (bone==9 && vertices!=144) ||
               (bone==12 && vertices!=12 && vertices!=90 && vertices!=192))continue;
        }
        ++matched_palettes;
        if(!p.ready) {++unpaired_matches;Diagnose(p,nullptr);return {};}
        Override next;next.count=s.key.count;
        // Rebuild in the exact native draw view. The separate projection replay
        // supplies camera motion; using the camera capture here also shifts the
        // actor whenever the game's draw view and camera snapshot differ.
        auto world=p.half;
        if(sample_fraction!=.5f) {
            const auto& old=p.previous;
            if(HierarchyModel(s.model)) {
                if(!old.skeleton_pose || !s.skeleton_pose)return {};
                SkeletonHalf temporary;
                if(!cache)temporary=BlendSkeleton(*old.skeleton_pose,*s.skeleton_pose,
                    s.model=="link_v2",s.model=="childlink_v2",sample_fraction);
                const auto& bones=cache?cache->Get(old,s,sample_fraction):temporary;
                for(unsigned i=0;i<next.count;++i) {
                    const auto id=s.bone_ids[i];
                    const auto skin=id<bones.size() && bones[id]?SkinBone(s,i,*bones[id]):std::nullopt;
                    if(!skin)return {};
                    world[i]=*skin;
                }
            } else for(unsigned i=0;i<next.count;++i) {
                const auto blend=BlendPalette(old,s,i,sample_fraction);
                if(!blend)return {};
                world[i]=*blend;
            }
        }
        for(unsigned i=0;i<next.count;++i)next.palette[i]=Math::Multiply(s.draw_view,world[i]);
        if(result) {
            if(result->count!=next.count && !SamePalettePrefix(selected->current,s) &&
               !GanondorfIntroPrefix(*selected,p) && !GanondorfIntroPrefix(p,*selected) &&
               !GraveyardMaskPrefix(*selected,p) && !GraveyardMaskPrefix(p,*selected) &&
               !SkinnedRootPrefix(*selected,p) && !SkinnedRootPrefix(p,*selected)) {
                ++ambiguous_matches;Diagnose(*selected,&p);return {};
            }
            for(unsigned i=0;i<std::min(result->count,next.count);++i)
                if(!PaletteNear(result->palette[i],next.palette[i])) {++ambiguous_matches;Diagnose(*selected,&p);return {};}
            // Shorter uploads leave unused GPU palette registers intact. Keep
            // the longest fully matched palette only after all overlapping
            // interpolated joints agree. Do not let a later prefix truncate it.
            if(result->count!=next.count)++shared_prefix_matches;
            if(result->count>=next.count)continue;
        }
        result=std::move(next);selected=&p;
    }
    return result;
}
class Replay {
    PhaseSkeletonCache phase_cache;
    std::uint64_t seen=~std::uint64_t{};
    std::vector<Pair> pairs;
public:
    bool SongEffect(const Palette& native,const CameraSample& camera,std::uint64_t frame,std::int64_t now) {
        if(!camera.valid)return false;
        if(seen!=revision.load()) {
            std::scoped_lock lock{mutex};pairs=history.Pairs();seen=revision.load();
        }
        bool found=false;
        for(const auto& p:pairs) {
            const auto& s=p.current;
            // PICA consumes the previous guest frame's submitted commands
            // before that frame's next skeleton update. The song capture
            // shows ~30ms between palette submission and GPU consumption.
            // Match that exact palette across this one-frame boundary too.
            if(s.frame>frame || frame-s.frame>1 || s.camera.epoch!=camera.epoch || s.camera.owner!=camera.owner ||
               s.camera.scene!=camera.scene || now<s.time || now-s.time>50000)continue;
            bool match=true;
            for(unsigned i=0;i<s.key.count && match;++i)match=PaletteNear(native[i],s.native[i]);
            if(!match)continue;
            if(!SongWidescreen::Model(s.model))return false;
            found=true;
        }
        return found;
    }
    std::optional<Override> Select(const Palette& native,const CameraSample& camera,
                                  std::uint64_t frame,std::int64_t now,unsigned vertices=0) {
        if(!enabled.load() || !camera.valid)return {};
        if(seen!=revision.load()) {
            std::scoped_lock lock{mutex};pairs=history.Pairs();seen=revision.load();
        }
        return Resolve(pairs,native,camera,frame,now,&phase_cache,vertices);
    }
};
}
