// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/title_hooks.h"

namespace OOT3D::CameraClearance {
using U32=std::uint32_t;
using Vec=std::array<float,3>;
inline Vec Add(Vec a,Vec b) {for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec Mul(Vec a,float s) {for(auto& x:a)x*=s;return a;}
inline float Dot(Vec a,Vec b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec Cross(Vec a,Vec b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
template<class M> void Put(M& m,U32 p,const Vec& v) {for(unsigned i=0;i<3;++i)m.Write(p+i*4,std::bit_cast<U32>(v[i]));}
template<class M> float Float(M& m,U32 p) {return std::bit_cast<float>(m.Read(p).value_or(0));}
struct Frustum {
    Vec at{},eye{},back{},right{},up{};
    float radius{},near{},half_width{},half_height{};
    static std::optional<Frustum> Make(Vec at,Vec eye,float near,float fov,float aspect) {
        if(!std::isfinite(near) || near<.1f || near>100 || !std::isfinite(fov) || fov<1 || fov>120 ||
           !std::isfinite(aspect) || aspect<1 || aspect>3)return {};
        Frustum f;f.at=at;f.eye=eye;f.near=near;
        f.back=Add(eye,Mul(at,-1));f.radius=std::sqrt(Dot(f.back,f.back));
        if(!std::isfinite(f.radius) || f.radius<near+2)return {};
        f.back=Mul(f.back,1/f.radius);
        f.right=Cross({0,1,0},f.back);
        const float horizontal=std::sqrt(Dot(f.right,f.right));
        if(horizontal<.05f)return {};
        f.right=Mul(f.right,1/horizontal);f.up=Cross(f.back,f.right);
        f.half_height=near*std::tan(fov*3.14159265358979323846f/360);
        f.half_width=f.half_height*aspect;return f;
    }
    Vec Corner(unsigned i) const {
        return Add(Add(eye,Mul(back,-near)),Add(Mul(right,(i&1?1.f:-1.f)*half_width),
                                                 Mul(up,(i&2?1.f:-1.f)*half_height)));
    }
    // Restrict only distance along the orbit arm. Never feed the correction
    // back into pitch/yaw or zero tangential stick velocity at contact.
    float Limit(Vec point,Vec normal,float previous,float* minimum=nullptr) const {
        const float length=std::sqrt(Dot(normal,normal));
        if(!std::isfinite(length) || length<.9f || length>1.1f)return previous;
        normal=Mul(normal,1/length);
        const float support=std::min(0.f,-near*Dot(normal,back)-half_width*std::abs(Dot(normal,right))-
                                       half_height*std::abs(Dot(normal,up)));
        const float slope=Dot(normal,back);
        const float base=Dot(normal,Add(at,Mul(point,-1)))+support-.75f;
        // A plane behind the pivot can also impose a minimum distance. Keep
        // that bound so a later wall cannot pull the view through this plane.
        if(minimum && slope>.0001f)*minimum=std::max(*minimum,-base/slope);
        if(slope>=-.0001f)return base+slope*previous>=0?previous:-1.f;
        const float bound=base/-slope;
        return bound>=near+1?std::min(previous,bound):-1.f;
    }
};

// All pending state, including volatile integer/FP registers, is on the guest
// stack. A restored savestate does not need the original host Hook instance.
constexpr U32 Size=0x1c0,Cookie=0x43465255,Done=0x4346444e;
constexpr U32 RequestedRay=0x43525130,RequestedContact=0x43525131;
constexpr U32 NativeCollision=0x3553fc;
template<class M> std::optional<Frustum> Read(M& m,U32 sp) {
    Vec at,eye;
    if(!TitleScreen::ReadFloats(m,sp+0x40,at) || !TitleScreen::ReadFloats(m,sp+0x4c,eye))return {};
    return Frustum::Make(at,eye,Float(m,sp+0x58),Float(m,sp+0x5c),Float(m,sp+0x60));
}
template<class M> void Contact(M& m,U32 sp,const Frustum& f,U32 query) {
    const float previous=Float(m,sp+0x64);
    if(previous<0 || !m.Read(query+0x18).value_or(0))return;
    Vec hit,normal;
    if(!TitleScreen::ReadFloats(m,query,hit) || !TitleScreen::ReadFloats(m,query+12,normal))return;
    const float normal_length=std::sqrt(Dot(normal,normal));
    if(!std::isfinite(normal_length) || normal_length<.9f || normal_length>1.1f)return;
    if(m.Read(sp+0x1bc)==RequestedRay)m.Write(sp+0x1bc,RequestedContact);
    // Camera_BGCheckInfo returns intersection + one unnormalized polygon normal.
    float minimum=Float(m,sp+0x1b8);
    const float bound=f.Limit(Add(hit,Mul(normal,-1)),normal,previous,&minimum);
    m.Write(sp+0x64,std::bit_cast<U32>(bound));
    m.Write(sp+0x1b8,std::bit_cast<U32>(minimum));
}
template<class M,class R> void Query(M& m,R& r,const Frustum& f,U32 continuation) {
    const U32 sp=r[13],index=m.Read(sp+0x68).value_or(4);
    for(unsigned i=0;i<0x28;i+=4)m.Write(sp+i,0);
    Put(m,sp,f.Corner(index));Put(m,sp+0x28,f.at);
    r[0]=m.Read(sp+0x3c).value_or(0);r[1]=sp+0x28;r[2]=sp;
    r[14]=continuation;r[15]=NativeCollision;
}
template<class M,class R,class F> bool Begin(M& m,R& r,F& fp,U32 flags,U32 fpscr,U32 cam,
                                           float near,float fov,float aspect,U32 epoch,U32 continuation,
                                           U32 requested_eye=0) {
    const U32 original=r[13];Vec at,eye;
    if(!TitleScreen::ReadFloats(m,requested_eye?requested_eye:original,eye) ||
       !TitleScreen::ReadFloats(m,original+0x28,at))return false;
    // Native BGCheckInfo snaps a near-floor request onto a floor-adjusted
    // point, even when the center ray is clear. That point must supply a
    // contact plane, not a new orbit direction that changes back next tick.
    const auto f=Frustum::Make(at,eye,near,fov,aspect);
    if(!f || original<Size || !TitleScreen::Pointer(original-Size,Size))return false;
    const U32 sp=original-Size;
    for(unsigned i=0;i<Size;i+=4)if(!m.Read(sp+i))return false;
    for(unsigned i=0;i<16;++i)m.Write(sp+0x70+i*4,r[i]);
    for(unsigned i=0;i<64;++i)m.Write(sp+0xb0+i*4,fp[i]);
    m.Write(sp+0x1b0,flags);m.Write(sp+0x1b4,fpscr);
    m.Write(sp+0x34,Cookie);m.Write(sp+0x38,sp^Cookie);m.Write(sp+0x3c,cam);
    Put(m,sp+0x40,at);Put(m,sp+0x4c,eye);
    m.Write(sp+0x58,std::bit_cast<U32>(near));m.Write(sp+0x5c,std::bit_cast<U32>(fov));
    m.Write(sp+0x60,std::bit_cast<U32>(aspect));
    m.Write(sp+0x64,std::bit_cast<U32>(f->radius-(requested_eye?1.f:0.f)));
    m.Write(sp+0x68,0);m.Write(sp+0x6c,epoch);
    m.Write(sp+0x1b8,std::bit_cast<U32>(near+1));
    m.Write(sp+0x1bc,requested_eye?RequestedRay:0);
    Contact(m,sp,*f,original);
    r[13]=sp;Query(m,r,*f,continuation);return true;
}
// Returns true while another query is pending; otherwise restores the initial
// collision result/CPU state and lets the original finish path run unchanged.
template<class M,class R,class F> bool Resume(M& m,R& r,F& fp,U32& flags,U32& fpscr,U32 epoch,U32 continuation) {
    const U32 sp=r[13];
    if(!TitleScreen::Pointer(sp,Size) || m.Read(sp+0x34)!=Cookie || m.Read(sp+0x38)!=(sp^Cookie))return false;
    const auto f=Read(m,sp);
    const bool valid=f && m.Read(sp+0x6c)==epoch;
    if(valid) {
        Contact(m,sp,*f,sp);
        const U32 index=m.Read(sp+0x68).value_or(4)+1;
        m.Write(sp+0x68,index);
        if(index<4) {Query(m,r,*f,continuation);return true;}
    }
    const float radius=Float(m,sp+0x64);
    for(unsigned i=0;i<16;++i)r[i]=m.Read(sp+0x70+i*4).value_or(0);
    for(unsigned i=0;i<64;++i)fp[i]=m.Read(sp+0xb0+i*4).value_or(0);
    flags=m.Read(sp+0x1b0).value_or(flags);fpscr=m.Read(sp+0x1b4).value_or(fpscr);
    const U32 ray=m.Read(sp+0x1bc).value_or(0);
    // Clear space keeps the exact native eye, including its one-unit inset.
    // With contact, fit the saved request against every detected plane.
    if(valid && ray!=RequestedRay && radius>=Float(m,sp+0x1b8) && radius<f->radius)
        Put(m,r[13],Add(f->at,Mul(f->back,radius)));
    m.Write(r[13]+0x1c,Done);m.Write(sp+0x34,0);
    return false;
}
}
