// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace OOT3D::Randomizer::GenerationProgress {
inline constexpr unsigned AttemptLimit=10;
struct Session {
    std::atomic<unsigned> attempt{1};
    void Observe(std::string_view text) {
        if(!text.empty() && text.back()=='\n')text.remove_suffix(1);
        if(text.empty())return;
        unsigned next=0;
        const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),next);
        if(error!=std::errc{} || end!=text.data()+text.size() || next<1 || next>AttemptLimit)return;
        auto old=attempt.load();
        while(old<next && !attempt.compare_exchange_weak(old,next)) {}
    }
};
// Workers keep their own session. A cancelled worker cannot update a new run.
inline Common::AtomicSharedPtr<Session> current;
inline auto Begin() {
    auto session=std::make_shared<Session>();current.store(session);return session;
}
inline void Reset() {current.store(std::shared_ptr<Session>{});}
inline unsigned Attempt() {const auto session=current.load();return session?session->attempt.load():1;}
// Presentation runs only on the emulation thread, independently of the worker.
struct Presentation {
    static constexpr std::int64_t HoldUs=5000000,FadeUs=150000;
    enum class Phase {Hold,Visible,Out,In};
    Phase phase=Phase::Hold;
    unsigned displayed=1;
    float opacity=1;
    std::int64_t began=0,changed=0;
    bool initialized=false;
    void Frame(std::int64_t now,unsigned latest) {
        if(!initialized || now<began || now<changed) {
            *this={};initialized=true;began=changed=now;
        }
        latest=std::clamp(latest,1u,AttemptLimit);
        if(phase==Phase::Hold) {
            if(now-began<HoldUs)return;
            phase=Phase::Visible;
        }
        const auto ease=[](float t) {return t*t*(3-2*t);};
        if(phase==Phase::Out) {
            opacity=1-ease(std::clamp(float(now-changed)/FadeUs,0.f,1.f));
            if(now-changed>=FadeUs) {
                // Switch only at zero opacity; coalesce any rapid retries.
                displayed=latest;phase=Phase::In;changed=now;opacity=0;
            }
        } else if(phase==Phase::In) {
            opacity=ease(std::clamp(float(now-changed)/FadeUs,0.f,1.f));
            if(now-changed>=FadeUs)phase=Phase::Visible;
        }
        if(phase==Phase::Visible && latest>displayed) {phase=Phase::Out;changed=now;}
    }
};
inline Presentation presentation;
inline std::shared_ptr<Session> presentation_session;
inline void UpdatePresentation(std::int64_t now,bool active) {
    const auto session=active?current.load():std::shared_ptr<Session>{};
    if(session!=presentation_session) {presentation={};presentation_session=session;}
    if(session)presentation.Frame(now,session->attempt.load());
}
inline unsigned DisplayedAttempt() {return presentation.displayed;}
inline float TransitionOpacity() {return presentation.opacity;}
inline std::string Label(unsigned attempt,unsigned language) {
    if(attempt<2 || attempt>AttemptLimit)
        return language==4 || language==5?"Randomisation...":language==6 || language==7?"Aleatorizando...":"Randomizing...";
    return (language==4 || language==5?"Tentative ":language==6 || language==7?"Intento ":"Attempt ")+
        std::to_string(attempt)+(language==4 || language==5?" sur ":language==6 || language==7?" de ":" of ")+
        std::to_string(AttemptLimit);
}
}
