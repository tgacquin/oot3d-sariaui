// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/interpolation_math.h"
#include <string_view>
namespace OOT3D::SongWidescreen {
// Exact CMB names in the original zelda_keep.zar; these are the song backdrops.
inline bool Model(std::string_view name) {
    return name=="efc_ocarina_1_m" || name=="efc_ocarina_2_m" ||
        name=="efc_ocarina_3_m" || name=="efc_ocarina_5_m" ||
        name=="efc_ocarina_6_m" || name=="efc_ocarina_41_" || name=="efc_ocarina_42_";
}
inline float Zoom(float native_fov,const Interpolation::Math::Matrix& projection) {
    if(!std::isfinite(native_fov) || native_fov<=1 || native_fov>=170 ||
       projection[3]!=std::array<double,4>{0,0,-1,0})return 1;
    // Projection is rotated for the 3DS screen. Row0 is the vertical lens.
    const double focal=std::hypot(projection[0][0],projection[0][1]);
    const double scale=1/(focal*std::tan(native_fov*3.141592653589793/360));
    return std::isfinite(scale)?float(std::clamp(scale,1.,3.)):1.f;
}
}
