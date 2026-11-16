// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/hud_layout.h"
#include "oot3d/hud_buttons.h"
#include "oot3d/dpad_layout.h"

namespace OOT3D::HUDDefaults {
// Authored layout snapshot. Missing settings use these; existing settings win.
inline HUD::Layout Basic() {
    HUD::Layout l;
    l[HUD::Hearts]={-12.f,0.f,0.629999995f};
    l[HUD::Rupees]={-13.6000004f,6.9000001f,0.670000017f};
    l[HUD::Magic]={-12.3999996f,-6.0999999f,0.50999999f};
    l[HUD::AButton]={-85.0999985f,-181.f,0.639999986f};
    l[HUD::LoadingIcon]={0.f,0.f,1.f};
    l[HUD::Minimap]={301.399994f,39.2000008f,0.899999976f};
    l[HUD::Keys]={-13.5f,12.1999998f,0.639999986f};
    l[HUD::Timer]={0.f,0.f,1.f};
    l[HUD::Minigame]={0.f,0.699999988f,1.f};
    l[HUD::ShardAgony]={-26.6000004f,68.9000015f,0.519999981f};
    l[HUD::TargetIndicator]={400.f,0.f,0.100000001f};
    l.outside_minimap={272.600006f,56.7999992f,0.899999976f};
    l.heart_spacing=-2.70000005f;
    l.magic_height=1.91999996f;
    l.magic_border=1.58000004f;
    l.magic_background_opacity=0.75f;
    return l;
}
inline HUDButtons::Layout Buttons() {
    HUDButtons::Layout l;
    l.item_x=68.f;
    l.item_y=-121.f;
    l.item_scale=0.600000024f;
    l.diamond_rotation=0.f;
    l.diamond_spacing=32.9000015f;
    l.item_labels={0.f,-2.4000001f,0.74000001f};
    l.item_icons={1.39999998f,-1.79999995f,1.34000003f};
    l.buttons[HUDButtons::B].pose={256.f,21.5f,0.5f};
    l.buttons[HUDButtons::X].pose={304.f,125.099998f,1.f};
    l.buttons[HUDButtons::Y].pose={271.100006f,158.f,1.f};
    l.buttons[HUDButtons::I].pose={304.f,190.899994f,1.f};
    l.buttons[HUDButtons::II].pose={336.899994f,158.f,1.f};
    return l;
}
inline DPad::Layout DPadLayout() {
    DPad::Layout l;
    l.group={-25.1000004f,-23.7000008f,0.600000024f,0.600000024f};
    return l;
}
} // namespace OOT3D::HUDDefaults
