// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_common/controller_autoconfig.h"
#include "oot3d/player_mode.h"
#include "oot3d/input.h"
#include "oot3d/player_graphics.h"
#include <cstdlib>
#include <iostream>
#include <source_location>

static void Check(bool condition, std::source_location where=std::source_location::current()) {
    if (!condition) {
        std::cerr<<"Check failed at line "<<where.line()<<"\n";
        std::exit(1);
    }
}

int main() {
    namespace G = OOT3D::PlayerGraphics;
    namespace I = OOT3D::Interpolation;
    struct Toggle {
        bool value=true, global=false;
        void SetGlobal(bool g) { global=g; }
        void SetValue(bool v) { value=v; }
        bool GetValue() const { return value; }
    };
    struct { Toggle use_vsync, use_disk_shader_cache, use_hw_shader, async_shader_compilation; } renderer;
    int applied=0,saved=0;
    const auto flush=[&] { G::ApplyPending(renderer,[&]{++applied;},[&]{++saved;}); };
    G::SelectMode(G::Mode::CPU); flush();
    Check(I::Replaying() && I::cpu_vertices && I::target_fps==60 && !I::match_refresh_rate);
    Check(G::CurrentMode()==G::Mode::CPU && !renderer.use_hw_shader.value);
    Check(renderer.use_hw_shader.global && applied==1 && saved==1);
    flush(); Check(applied==1 && saved==1); // Consumed once, no per-tick saves.
    I::target_fps=120;
    G::SelectMode(G::Mode::GPU); flush();
    Check(I::target_fps==120 && !I::match_refresh_rate && I::TargetFPS()==120);
    Check(I::Replaying() && !I::cpu_vertices && renderer.use_hw_shader.value);
    Check(G::CurrentMode()==G::Mode::GPU);
    I::match_refresh_rate=true;I::display_refresh_rate=144;
    G::SelectMode(G::Mode::Original); flush();
    Check(!I::Replaying() && G::CurrentMode()==G::Mode::Original);
    Check(I::target_fps==120 && I::match_refresh_rate);
    G::SelectMode(G::Mode::CPU); flush();
    Check(I::Replaying() && I::target_fps==120 && I::match_refresh_rate);
    Check(I::TargetFPS()==120); // Display matching keeps the renderer's cap.
    I::display_refresh_rate=75;
    Check(I::TargetFPS()==75);
    G::SelectMode(G::Mode::Original); flush();
    const int before=applied;
    G::set_hardware=0; G::set_async=0; flush();
    Check(!renderer.use_hw_shader.value && !renderer.async_shader_compilation.value);
    Check(applied==before); // No renderer/game restart for shader preferences.
    G::set_vsync=0; G::set_cache=0; flush();
    Check(!renderer.use_vsync.value && !renderer.use_disk_shader_cache.value);
    Check(applied==before+1 && !G::vsync && !G::cache);

    OOT3D::available=false;
    Check(!OOT3D::HandleSettingsFunctionKey(1,false));
    OOT3D::available=true;
    Check(OOT3D::HandleSettingsFunctionKey(1,false));
    Check(OOT3D::visible && (OOT3D::settings_view.load()==OOT3D::SettingsView::Player)==OOT3D::IsPlayerBuild());
    Check(OOT3D::HandleSettingsFunctionKey(1,true));
    Check(OOT3D::visible); // Holding the key cannot repeatedly toggle the menu.
    Check(OOT3D::HandleSettingsFunctionKey(1,false));
    Check(!OOT3D::visible);
    Check(OOT3D::HandleSettingsFunctionKey(2,false));
    Check(OOT3D::visible==!OOT3D::IsPlayerBuild());
    if (!OOT3D::IsPlayerBuild()) Check((OOT3D::settings_view.load()==OOT3D::SettingsView::Player));
    if (OOT3D::IsPlayerBuild()) {
        Check(!OOT3D::HandleSettingsFunctionKey(3,false) && !OOT3D::visible);
    } else {
        Check(OOT3D::HandleSettingsFunctionKey(3,false) && OOT3D::visible && (OOT3D::settings_view.load()==OOT3D::SettingsView::Defaults));
        Check(OOT3D::HandleSettingsFunctionKey(3,true) && OOT3D::visible);
        Check(OOT3D::HandleSettingsFunctionKey(2,false) && OOT3D::visible && !(OOT3D::settings_view.load()==OOT3D::SettingsView::Defaults));
        Check(OOT3D::HandleSettingsFunctionKey(3,false) && (OOT3D::settings_view.load()==OOT3D::SettingsView::Defaults));
        Check(OOT3D::HandleSettingsFunctionKey(3,false) && !OOT3D::visible);
    }
    OOT3D::available=false; OOT3D::visible=false;
    using namespace InputCommon::ControllerAutoConfig;
    using Common::ParamPackage;
    Settings::InputProfile keyboard{};
    keyboard.name="Keyboard";
    keyboard.maptype=Settings::InputMappingType::GuidPort;
    for(unsigned i=0;i<keyboard.buttons.size();++i)
        keyboard.buttons[i]=ParamPackage{{"engine","keyboard"},{"code",std::to_string(i)}}.Serialize();
    for(unsigned i=0;i<keyboard.analogs.size();++i)
        keyboard.analogs[i]=ParamPackage{{"engine","analog_from_button"},{"up",std::to_string(i)}}.Serialize();
    const auto buttons=keyboard.buttons;
    const auto analogs=keyboard.analogs;
    const ParamPackage controller{{"engine","sdl"},{"api","controller"},{"guid","first"},{"port","0"},{"active","1"}};
    const auto button=[](ParamPackage device,int index) {
        device.Erase("active"); device.Set("button",index); return device;
    };
    const auto analog=[](ParamPackage device,int index) {
        device.Erase("active"); device.Erase("button");
        device.Set("axis_x",index*2);device.Set("axis_y",index*2+1);return device;
    };
    Check(!Select(keyboard,{},buttons,analogs,button,analog));
    auto mapped=Select(keyboard,{controller},buttons,analogs,button,analog);
    Check(mapped.has_value());
    Check(Managed(*mapped,button,analog));
    Check(keyboard.buttons==buttons && keyboard.analogs==analogs);
    Check(!Select(*mapped,{controller},buttons,analogs,button,analog));
    // A new active controller is selected when an automatically mapped device leaves.
    auto other=controller;other.Set("guid","second");
    auto hotplug=Select(*mapped,{other},buttons,analogs,button,analog);
    Check(hotplug.has_value());
    Check(ParamPackage(hotplug->buttons[Settings::NativeButton::A]).Get("guid","")=="second");
    // Even a single deliberate binding edit disables automatic replacement.
    mapped->buttons[Settings::NativeButton::A]=buttons[Settings::NativeButton::A];
    Check(!Select(*mapped,{other},buttons,analogs,button,analog));
    auto custom=keyboard; custom.buttons[0]=ParamPackage{{"engine","keyboard"},{"code","999"}}.Serialize();
    Check(!Select(custom,{controller},buttons,analogs,button,analog));
    const auto incomplete=[&](ParamPackage device,int index) {
        return index==Settings::NativeButton::A?ParamPackage{}:button(device,index);
    };
    Check(!Select(keyboard,{controller},buttons,analogs,incomplete,analog));
    for(auto action:{"Fullscreen","Exit Fullscreen","Toggle Custom Textures","Toggle Minimap","First-Person View"})
        Check(OOT3D::PlayerShortcutAllowed(action));
    for(auto action:{"Load File","Load Amiibo","Toggle Screen Layout","Multiplayer Browse Public Rooms","Advance Frame","Restart Emulation"})
        Check(!OOT3D::PlayerShortcutAllowed(action));
    std::cout<<"F1/F2 routing, controller initialization/hotplug and shortcut checks passed ("
             <<(OOT3D::IsPlayerBuild()?"player":"developer")<<")\n";
}
