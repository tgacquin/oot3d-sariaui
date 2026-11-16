// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/camera_clearance.h"

namespace OOT3D::CameraClearance {
// Shorten the next REQUEST, never the collision-tested result. Even a closer
// eye needs new center/corner queries: a plane can impose a minimum distance.
struct Recovery {
    Vec previous_at{};
    float distance{},requested{},intended{};
    std::uint64_t frame{};
    unsigned hold{};
    bool valid{},pending{},recovering{};
    void Reset() {*this={};}
    Vec Request(Vec at,Vec eye,std::uint64_t next_frame) {
        const Vec delta=Add(eye,Mul(at,-1));
        const float length=std::sqrt(Dot(delta,delta));
        const Vec movement=Add(at,Mul(previous_at,-1));
        if(!std::isfinite(length) || length<1) {Reset();return eye;}
        if(!valid || next_frame!=frame+1 || Dot(movement,movement)>32.f*32.f)Reset();
        intended=requested=length;
        if(valid && recovering && length>distance) {
            // Two quiet native updates before returning, then at most 75
            // units/sec at 30 Hz. Taper the final approach over ~0.2 seconds.
            const float step=hold?0.f:std::min(2.5f,(length-distance)*.25f);
            requested=std::min(length,distance+step);
            if(length-requested<.05f)requested=length;
        }
        previous_at=at;frame=next_frame;pending=true;
        return requested==length?eye:Add(at,Mul(delta,requested/length));
    }
    void Observe(Vec at,Vec eye,std::uint64_t current_frame) {
        if(!pending || current_frame!=frame) {Reset();return;}
        pending=false;
        const auto delta=Add(eye,Mul(at,-1));
        distance=std::sqrt(Dot(delta,delta));
        if(!std::isfinite(distance) || distance<1) {Reset();return;}
        // Camera_BGCheckInfo's clear-ray result is one unit inside the
        // requested eye. Convert back to request distance before feeding it
        // into another query, otherwise a clear ray would creep inward.
        distance+=1.f;
        if(distance<requested-.75f)hold=2;
        else if(hold)--hold;
        recovering=distance<intended-.05f;
        valid=true;previous_at=at;
    }
};
}
