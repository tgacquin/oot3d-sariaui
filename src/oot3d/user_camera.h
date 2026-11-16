// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace OOT3D::UserCamera {
// F1 owns the authored camera values. F2 only stores relative adjustments.
enum Scale { Fov, Distance, Sensitivity, Acceleration, Deceleration, ScaleCount };
enum Toggle { SmoothPullback, InvertX, InvertY, ToggleCount };
constexpr std::array ScaleNames{"fov", "distance", "sensitivity", "acceleration", "deceleration"};
constexpr std::array ToggleNames{"smooth_pullback", "invert_x", "invert_y"};
constexpr bool FramingScale(Scale setting) {return setting==Fov || setting==Distance;}
constexpr float MinimumScale(Scale setting) {return FramingScale(setting)?.75f:.25f;}
constexpr float MaximumScale(Scale setting) {return FramingScale(setting)?1.25f:4.f;}
struct Values {
    std::array<float,ScaleCount> scales{1,1,1,1,1};
    std::array<int,ToggleCount> toggles{-1,-1,-1}; // -1 inherits F1.
    bool operator==(const Values&) const = default;
};
inline std::array<std::atomic<float>,ScaleCount> scales{1.f,1.f,1.f,1.f,1.f};
inline std::array<std::atomic<int>,ToggleCount> toggles{-1,-1,-1};
inline std::atomic<bool> dirty{false};
inline float SanitizeScale(Scale setting,float value) {
    return std::isfinite(value)?std::clamp(value,MinimumScale(setting),MaximumScale(setting)):1.f;
}
inline int SanitizeToggle(int value) {return value>=-1 && value<=1?value:-1;}
inline Values Get() {
    Values value;
    for(unsigned i=0;i<ScaleCount;++i)value.scales[i]=scales[i].load();
    for(unsigned i=0;i<ToggleCount;++i)value.toggles[i]=toggles[i].load();
    return value;
}
inline void Set(Values value,bool save=true) {
    for(unsigned i=0;i<ScaleCount;++i)scales[i]=SanitizeScale(static_cast<Scale>(i),value.scales[i]);
    for(unsigned i=0;i<ToggleCount;++i)toggles[i]=SanitizeToggle(value.toggles[i]);
    if(save)dirty=true;
}
inline float Apply(Scale setting,float authored,float low,float high) {
    return std::clamp(authored*SanitizeScale(setting,scales[setting].load()),low,high);
}
inline bool Resolve(int override_value,bool authored) {
    return override_value<0?authored:override_value!=0;
}
inline bool Apply(Toggle setting,bool authored) {
    return Resolve(toggles[setting].load(),authored);
}
inline void ResetOrbit(Values& value) {
    for(auto setting:{Sensitivity,Acceleration,Deceleration})value.scales[setting]=1;
    value.toggles.fill(-1);
}
} // namespace OOT3D::UserCamera
