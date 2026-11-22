// Emulator-side replay adapted from DekUI. No guest patch implementation is used.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_state.h"
#include "oot3d/gyro_aim.h"
namespace OOT3D::Interpolation {
constexpr std::uint64_t ModelShader=0x6352a3be33cb99c6ULL, ModelSwizzle=0xba817d0ef27276d0ULL;
// CmbVShader.shbin's DVLE uniform table (register numbers minus the 16 inputs):
// uProjection=f0..3, uModelView=f4..7, uMatrixPallet=f20..49, uInvView=f76..79.
constexpr unsigned InverseViewRow=76;
inline bool Perspective(const Matrix& p) {
    return Math::Near(p,p,0) && p[3]==std::array<double,4>{0,0,-1,0};
}
inline bool SameLens(const Matrix& a,const Matrix& b) {
    // Near/far planes may differ between sky and scene.
    for(unsigned r:{0u,1u,3u})for(unsigned c=0;c<4;++c)
        if(std::abs(a[r][c]-b[r][c])>.00001)return false;
    return true;
}
inline bool ContinuousLens(const Matrix& a,const Matrix& b) {
    // Accept gradual zooms without treating unrelated projections as history.
    // Bound each image-plane row's change to 25% of its smaller focal scale.
    // This also rejects flipped/rotated lenses and degenerate image planes.
    // Depth coefficients are per draw and deliberately excluded.
    for(unsigned r=0;r<2;++r) {
        double before=0,after=0,change=0;
        for(unsigned c=0;c<2;++c) {
            before+=a[r][c]*a[r][c];after+=b[r][c]*b[r][c];
        }
        for(unsigned c=0;c<4;++c)change+=std::pow(a[r][c]-b[r][c],2);
        if(std::min(before,after)<1e-12 || change>std::min(before,after)*.0625)return false;
    }
    return true;
}
inline bool CameraMatches(const Matrix& camera,const CameraSample& sample) {
    if(!sample.valid || !Math::Rigid(camera) || !Math::Rigid(sample.view))return false;
    const auto expected=Math::InverseCamera(sample.view);
    if(!expected)return false;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
        if(std::abs(camera[r][c]-(*expected)[r][c])>(c==3?.25:.002))return false;
    return true;
}
struct ColorClears {
    struct Fill { std::uint32_t start{},end{},rgba{}; };
    std::array<Fill,16> fills{};
    std::optional<std::uint64_t> frame;
    void Reset() { fills={};frame.reset(); }
    void Invalidate(std::uint32_t start,std::uint64_t end) {
        for(auto& fill:fills)
            if(start<fill.end && fill.start<end)fill={};
    }
    void Observe(std::uint64_t current,std::uint32_t start,std::uint32_t end,
                 std::uint32_t rgba,unsigned bytes) {
        if(frame!=current) { Reset();frame=current; }
        if(end<=start)return;
        Invalidate(start,end);
        if(bytes!=4)return;
        for(auto& fill:fills)if(fill.end==0) { fill={start,end,rgba};return; }
        // A full tracker may lose a clear, but cannot retain an overwritten one.
    }
    std::array<float,4> Background(std::uint64_t current,std::uint32_t address,
                                   unsigned width,unsigned height,bool rgba8) const {
        if(frame==current && rgba8 && width && height) {
            const std::uint64_t end=std::uint64_t(address)+std::uint64_t(width)*height*4;
            for(const auto& fill:fills)if(fill.start==address && fill.end==end) {
                // PICA RGBA8 is stored with R in the high byte. Private scene
                // coverage stays opaque; the native clear's alpha is not HUD coverage.
                return {float((fill.rgba>>24)&255)/255.f,float((fill.rgba>>16)&255)/255.f,
                        float((fill.rgba>>8)&255)/255.f,1.f};
            }
        }
        return {0,0,0,1};
    }
};
// A native projection may have a rotated/scaled perspective divide after
// View's distortion routine (2cf9e0). Only accept it by matching a projection
// captured from the same gameplay View, never by relaxing arbitrary draws.
inline bool NativeLensMatches(const Matrix& projection,const Matrix& native) {
    if(!Math::Near(projection,projection,0) || !Math::Near(native,native,0) ||
       std::abs(native[3][3])>1e-6 || native[3][2]>=-.25 || native[3][2]<-4 ||
       native[2][3]<=0)return false;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
        if(std::abs(projection[row][col]-native[row][col])>
           .00004*std::max(1.,std::abs(native[row][col])))return false;
    return true;
}
inline bool WorldProjection(const Matrix& projection,const CameraSample& sample) {
    if(Perspective(projection))return true;
    if(sample.valid && sample.projections_valid)
        for(const auto& native:sample.projections)if(NativeLensMatches(projection,native))return true;
    return false;
}
class ReplayTrack {
    Matrix previous_camera{},lens{},midpoint_lens{},delta{};
    CameraSample previous_sample{};
    std::uint64_t frame{};
    bool seen{},ready{};
    bool whole_lens{};
    Matrix Correct(const Matrix& projection) const {
        auto midpoint=whole_lens?midpoint_lens:projection;
        midpoint[0]=midpoint_lens[0];midpoint[1]=midpoint_lens[1];
        return Math::Multiply(midpoint,delta);
    }
public:
    ReplayFailure failure=ReplayFailure::None;
    void Reset() {seen=ready=false;}
    std::optional<Matrix> Observe(std::uint64_t next,const CameraSample& sample,
                                 const Matrix& camera,const Matrix& projection,std::int64_t now,
                                 bool verified_lens=false) {
        if(!sample.valid || sample.epoch!=generation.load() || now<sample.time || now-sample.time>100000) {
            failure=ReplayFailure::Sample;return {};
        }
        if((!verified_lens && !Perspective(projection)) || !CameraMatches(camera,sample)) {
            failure=ReplayFailure::CameraMismatch;return {};
        }
        if(seen && next==frame) {
            if(!ready)return {};
            if(whole_lens!=verified_lens || (whole_lens?!Math::Near(lens,projection,.0001):
                !SameLens(lens,projection))) {failure=ReplayFailure::DrawLens;return {};}
            if(!Math::Near(previous_camera,camera,.002)) {failure=ReplayFailure::DrawCamera;return {};}
            failure=ReplayFailure::None;
            return Correct(projection);
        }
        failure=ReplayFailure::History;
        ready=seen && whole_lens==verified_lens && next==frame+1 && sample.owner==previous_sample.owner &&
            sample.scene==previous_sample.scene && sample.epoch==previous_sample.epoch &&
            sample.cut_group==previous_sample.cut_group &&
            !(previous_sample.first_person && !sample.first_person) &&
            sample.time>=previous_sample.time && sample.time-previous_sample.time<=100000;
        if(ready && !ContinuousLens(lens,projection)) {ready=false;failure=ReplayFailure::LensCut;}
        if(ready && verified_lens)for(unsigned row=2;row<4;++row) {
            double size=0,change=0;
            for(unsigned col=0;col<4;++col) {
                size+=lens[row][col]*lens[row][col];
                change+=std::pow(lens[row][col]-projection[row][col],2);
            }
            if(size<1e-12 || change>size*.0625) {ready=false;failure=ReplayFailure::LensCut;}
        }
        if(ready) {
            double distance=0,trace=0;
            for(unsigned r=0;r<3;++r) {
                distance+=std::pow(previous_camera[r][3]-camera[r][3],2);
                for(unsigned c=0;c<3;++c)trace+=previous_camera[r][c]*camera[r][c];
            }
            const auto d=distance_limit.load(),a=angle_limit.load();
            ready=std::isfinite(d) && std::isfinite(a) && distance<=double(d)*d &&
                (trace-1)/2>=std::cos(a*3.141592653589793/180.);
            if(ready) {
                auto view=Math::Blend(previous_camera,camera,sample_fraction);
                if(GyroAim::enabled.load() && sample.aim_mode && sample.aim_mode==previous_sample.aim_mode) {
                    const float rotation=GyroAim::history.RotationPhase(sample.epoch,previous_sample.time,sample.time,sample_fraction);
                    if(rotation!=sample_fraction) {
                        const auto aim=Math::Blend(previous_camera,camera,rotation);
                        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)view[r][c]=aim[r][c];
                    }
                }
                delta=Math::Multiply(Math::InverseRigid(view),camera);
                for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                    midpoint_lens[r][c]=SampleMix(lens[r][c],projection[r][c]);
                failure=ReplayFailure::None;
            } else failure=ReplayFailure::CameraCut;
        }
        frame=next;seen=true;whole_lens=verified_lens;
        previous_camera=camera;previous_sample=sample;lens=projection;
        return ready?std::optional{Correct(projection)}:std::nullopt;
    }
};
class ReplayCamera {
    ReplayTrack legacy;
    std::array<ReplayTrack,6> native_tracks;
public:
    ReplayFailure failure=ReplayFailure::None;
    void Reset() {
        legacy.Reset();for(auto& track:native_tracks)track.Reset();
        failure=ReplayFailure::None;
    }
    std::optional<Matrix> Observe(std::uint64_t frame,const CameraSample& sample,
                                 const Matrix& camera,const Matrix& projection,std::int64_t now) {
        if(sample.projections_valid) {
            auto selected=sample;
            unsigned view=0;
            if(!CameraMatches(camera,selected) && sample.attached_valid) {
                selected.view=sample.attached_view;view=1;
            }
            if(CameraMatches(camera,selected)) {
                std::optional<unsigned> matched;
                double best=1e100;
                for(unsigned lens=0;lens<sample.projections.size();++lens) {
                    if(!NativeLensMatches(projection,sample.projections[lens]))continue;
                    double error=0;
                    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                        error+=std::pow((projection[row][col]-sample.projections[lens][row][col])/
                            std::max(1.,std::abs(projection[row][col])),2);
                    if(error<best) {best=error;matched=lens;}
                }
                if(matched) {
                    ++native_lens_draws;
                    if(!Perspective(projection))++warped_lens_draws;
                    if(view)++attached_view_draws;
                    auto& track=native_tracks[view*3+*matched];
                    auto result=track.Observe(frame,selected,camera,projection,now,true);
                    failure=track.failure;return result;
                }
            }
        }
        // Unknown projections retain the original strict guard; they cannot
        // silently enter one of the verified native lens histories.
        auto result=legacy.Observe(frame,sample,camera,projection,now);
        failure=legacy.failure;return result;
    }
};
// Completeness belongs to a guest frame, not the latest successful draw. One
// failed replay invalidates the pair; later HUD submissions cannot un-fail it.
struct ReplayFrame {
    std::optional<std::uint64_t> frame;
    std::uint32_t target{};
    unsigned epoch{};
    bool failed{},world{},menu{};
    MenuPage page=MenuPage::None;
    void Reset() {*this={};}
    void Fail(ReplayFailure reason) {
        if(!failed) {++replay_failures[unsigned(reason)];last_failure=reason;}
        failed=true;
    }
    bool Begin(std::uint64_t next,std::uint32_t address) {
        if(frame!=next || epoch!=generation.load()) {
            *this={next,address,generation.load(),false,false,false,menu_page.load()};return true;
        }
        if(target!=address)Fail(ReplayFailure::Target);
        return false;
    }
    bool Complete(std::uint64_t completed) const {
        return Replaying() && frame && completed && *frame==completed-1 &&
            epoch==generation.load() && page==menu_page.load() && (world || menu) && !failed;
    }
};
}
