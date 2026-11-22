// Copyright 2015-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/orbit_camera.h"
#include "oot3d/gyro_aim.h"
#include "oot3d/pause_input.h"
#include "oot3d/death_menu.h"
#include "oot3d/control_swap.h"
#include "oot3d/arrow_cycle.h"
#include "oot3d/item_equip_input.h"
#include "oot3d/item_use_input.h"
#include "oot3d/interpolation.h"
#include "oot3d/ocarina_input.h"
#include "oot3d/dpad_input.h"
#include "oot3d/qol_hooks.h"
#include "oot3d/input.h"
#include "oot3d/minimap_control.h"
#include "core/memory.h"
#endif
#include <algorithm>
#include <cmath>
#include <numeric>
#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/service.h"
#include "core/movie.h"
#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/input_latency.h"
#include "mm3d/classic_saving.h"
#include "mm3d/ocarina_input.h"
#include "mm3d/file_select_input.h"
#endif

SERVICE_CONSTRUCT_IMPL(Service::HID::Module)
SERIALIZE_EXPORT_IMPL(Service::HID::Module)

namespace Service::HID {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int file_version) {
    DEBUG_SERIALIZATION_POINT;
    ar & shared_mem;
    ar & event_pad_or_touch_1;
    ar & event_pad_or_touch_2;
    ar & event_accelerometer;
    ar & event_gyroscope;
    ar & event_debug_pad;
    ar & next_pad_index;
    ar & next_touch_index;
    ar & next_accelerometer_index;
    ar & next_gyroscope_index;
    ar & enable_accelerometer_count;
    ar & enable_gyroscope_count;
    if (Archive::is_loading::value) {
        LoadInputDevices();
#ifdef ENABLE_OOT3D_PRESENTATION
        OOT3D::OrbitCamera::reset_epoch.fetch_add(1);
        OOT3D::Interpolation::generation.fetch_add(1);
        OOT3D::QOL::Reset();
        OOT3D::ItemEquip::input={};
        OOT3D::ItemUse::input={};
        OOT3D::PauseMenu::menu_input={};
        OOT3D::DPad::input={};
#endif
    }
    ar & state.hex;
    ar & circle_pad_old_x;
    ar & circle_pad_old_y;
    // Update events are set in the constructor
    // Devices are set from the implementation (and are stateless afaik)
}
SERIALIZE_IMPL(Module)

ArticBaseController::ArticBaseController(
    const std::shared_ptr<Network::ArticBase::Client>& client) {

    udp_stream =
        client->NewUDPStream("ArticController", sizeof(ArticBaseController::ControllerData),
                             std::chrono::milliseconds(2));
    if (udp_stream.get()) {
        udp_stream->Start();
    }
}

ArticBaseController::ControllerData ArticBaseController::GetControllerData() {

    if (udp_stream.get() && udp_stream->IsReady()) {
        auto data = udp_stream->GetLastPacket();
        if (data.size() == sizeof(ControllerData)) {
            u32 id = *reinterpret_cast<u32*>(data.data());
            if ((id - last_packet_id) < (std::numeric_limits<u32>::max() / 2)) {
                last_packet_id = id;
                memcpy(&last_controller_data, data.data(), data.size());
            }
        }
    }
    return last_controller_data;
}

constexpr float accelerometer_coef = 512.0f; // measured from hw test result
constexpr float gyroscope_coef = 14.375f; // got from hwtest GetGyroscopeLowRawToDpsCoefficient call

DirectionState GetStickDirectionState(s16 circle_pad_x, s16 circle_pad_y) {
    // 30 degree and 60 degree are angular thresholds for directions
    constexpr float TAN30 = 0.577350269f;
    constexpr float TAN60 = 1 / TAN30;
    // a circle pad radius greater than 40 will trigger circle pad direction
    constexpr int CIRCLE_PAD_THRESHOLD_SQUARE = 40 * 40;
    DirectionState state{false, false, false, false};

    if (circle_pad_x * circle_pad_x + circle_pad_y * circle_pad_y > CIRCLE_PAD_THRESHOLD_SQUARE) {
        float t = std::abs(static_cast<float>(circle_pad_y) / circle_pad_x);

        if (circle_pad_x != 0 && t < TAN60) {
            if (circle_pad_x > 0)
                state.right = true;
            else
                state.left = true;
        }

        if (circle_pad_x == 0 || t > TAN30) {
            if (circle_pad_y > 0)
                state.up = true;
            else
                state.down = true;
        }
    }

    return state;
}

void Module::LoadInputDevices() {
    LOG_DEBUG(Frontend, "Loading input devices");
    std::transform(Settings::values.current_input_profile.buttons.begin() +
                       Settings::NativeButton::BUTTON_HID_BEGIN,
                   Settings::values.current_input_profile.buttons.begin() +
                       Settings::NativeButton::BUTTON_HID_END,
                   buttons.begin(), Input::CreateDevice<Input::ButtonDevice>);
    circle_pad = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.current_input_profile.analogs[Settings::NativeAnalog::CirclePad]);
#ifdef ENABLE_OOT3D_PRESENTATION
    oot_c_stick = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.current_input_profile.analogs[Settings::NativeAnalog::CStick]);
    oot_zl=Input::CreateDevice<Input::ButtonDevice>(Settings::values.current_input_profile.buttons[Settings::NativeButton::ZL]);
    oot_zr=Input::CreateDevice<Input::ButtonDevice>(Settings::values.current_input_profile.buttons[Settings::NativeButton::ZR]);
    OOT3D::ControlSwap::file_menu_shoulders=-1;
    OOT3D::ItemEquip::input={};
    OOT3D::ItemUse::input={};
    OOT3D::PauseMenu::menu_input={};
    OOT3D::DPad::input={};
    OOT3D::DPad::look_requested=false;
    OOT3D::OrbitCamera::stick_x.store(0);
    OOT3D::OrbitCamera::stick_y.store(0);
#endif
    motion_device = Input::CreateDevice<Input::MotionDevice>(
        Settings::values.current_input_profile.motion_device);
    touch_device = Input::CreateDevice<Input::TouchDevice>(
        Settings::values.current_input_profile.touch_device);
    if (Settings::values.current_input_profile.use_touchpad &&
        Settings::values.current_input_profile.controller_touch_device != "") {
        controller_touch_device = Input::CreateDevice<Input::TouchDevice>(
            Settings::values.current_input_profile.controller_touch_device);
    } else {
        controller_touch_device.reset();
    }
    if (Settings::values.current_input_profile.use_touch_from_button) {
        touch_btn_device = Input::CreateDevice<Input::TouchDevice>("engine:touch_from_button");
    } else {
        touch_btn_device.reset();
    }
}

void Module::UpdatePadCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    if (is_device_reload_pending.exchange(false))
        LoadInputDevices();

#ifdef ENABLE_OOT3D_PRESENTATION
    // Vanilla OoT never initializes IRRST. Poll its configured C-stick here.
    const auto [orbit_x,orbit_y] = oot_c_stick->GetStatus();
    const bool orbit_live = system.Movie().GetPlayMode() == Core::Movie::PlayMode::None;
    OOT3D::OrbitCamera::input_allowed.store(orbit_live);
    OOT3D::OrbitCamera::stick_x.store(orbit_live ? orbit_x : 0);
    OOT3D::OrbitCamera::stick_y.store(orbit_live ? orbit_y : 0);
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
    const auto poll_song_cursor=[&] {
        OOT3D::Ocarina::input.PollDirections(state.hex&0xf0,(state.hex>>24)&0xf0,
            system.CoreTiming().GetGlobalTimeUs().count());
    };
    bool menu_touch=false;
    const auto menu_input=[&](unsigned triggers,s16 stick_x,s16 stick_y) {
        struct GuestMemory { Memory::MemorySystem& memory;
            std::optional<u32> Read(u32 a) {return memory.Read32OrNullopt(a);}
        } guest{system.Memory()};
        const auto play=guest.Read(OOT3D::PauseMenu::UI+0xc).value_or(0);
        OOT3D::ControlSwap::shoulder_input.Filter(state.hex,triggers,orbit_live);
        OOT3D::ControlSwap::input.Filter(state.hex,orbit_live &&
            OOT3D::TitleScreen::Pointer(play,0x104) &&
            (guest.Read(play+0x100).value_or(0)&255)==3 && OOT3D::DPad::CodeMatches(guest),
            OOT3D::Ocarina::State(guest)!=0);
        OOT3D::DeathMenu::PollStart((state.hex&8)!=0);
        if(!orbit_live || OOT3D::visible.load() || !OOT3D::DPad::CodeMatches(guest) ||
           !OOT3D::DPad::Gameplay(guest))OOT3D::MinimapControl::Consume(false);
        // Timestamp shelf navigation at the 234 Hz HID poll, before gameplay
        // consumes the D-pad. Circle-pad units here range from -154 to 154.
        if(OOT3D::GyroAim::enabled.load()) {
            using namespace OOT3D::Interpolation;
            const auto camera=CameraSnapshot();
            if(orbit_live && !OOT3D::visible.load() && Replaying() && menu_page==MenuPage::None &&
               camera.valid && camera.aim_mode && camera.epoch==generation.load() &&
               std::abs(stick_x)<16 && std::abs(stick_y)<16 &&
               !(artic_controller && artic_controller->IsReady()) && motion_device) {
                const auto rate=std::get<1>(motion_device->GetStatus());
                OOT3D::GyroAim::history.Add(camera.epoch,system.CoreTiming().GetGlobalTimeUs().count(),
                                            {rate.x,rate.y,rate.z});
            } else OOT3D::GyroAim::history.Reset();
        }
        const u32 shop_stick=(stick_x>30?0x10u:stick_x< -30?0x20u:0u) |
                             (stick_y>30?0x40u:stick_y< -30?0x80u:0u);
        OOT3D::DPad::input.Filter(guest,state.hex,orbit_live && !OOT3D::visible.load(),
            system.CoreTiming().GetGlobalTimeUs().count(),shop_stick);
        OOT3D::Ocarina::input.Filter(guest,state.hex,orbit_live && !OOT3D::visible.load(),triggers);
        OOT3D::ArrowCycle::input.Poll(guest,state.hex,orbit_live && !OOT3D::visible.load() &&
            OOT3D::Ocarina::State(guest)==0,system.CoreTiming().GetGlobalTimeUs().count());
        OOT3D::ItemEquip::input.Poll(guest,triggers,state.hex,orbit_live && !OOT3D::visible.load(),
            system.CoreTiming().GetGlobalTimeUs().count());
        OOT3D::ItemUse::input.Poll(guest,triggers,state.hex,orbit_live && !OOT3D::visible.load() &&
            OOT3D::Ocarina::State(guest)==0,
            system.CoreTiming().GetGlobalTimeUs().count());
        OOT3D::QOL::Poll(guest,state.hex,orbit_live && !OOT3D::visible.load(),
            system.CoreTiming().GetGlobalTimeUs().count());
        menu_touch=OOT3D::PauseMenu::menu_input.Filter(guest,state.hex,orbit_live);
    };
#endif
    using namespace Settings::NativeButton;

    if (artic_controller.get() && artic_controller->IsReady()) {
        constexpr u32 HID_VALID_KEYS = 0xF0003FFF;
        constexpr u32 LIBCTRU_TOUCH_KEY = (1 << 20);

        ArticBaseController::ControllerData data = artic_controller->GetControllerData();

        state.hex = data.pad & HID_VALID_KEYS;

        s16 circle_pad_x = data.c_pad_x;
        s16 circle_pad_y = data.c_pad_y;

#ifdef ENABLE_OOT3D_PRESENTATION
        menu_input((data.pad>>14)&3,circle_pad_x,circle_pad_y);
#endif
        system.Movie().HandlePadAndCircleStatus(state, circle_pad_x, circle_pad_y);
#ifdef ENABLE_MM3D_PRESENTATION
        MM3D::OcarinaInput::Filter(state.hex);
        MM3D::ClassicSaving::FilterArrivalPad(state.hex,circle_pad_x,circle_pad_y);
        MM3D::FileSelectInput::Filter(state.hex,circle_pad_x,circle_pad_y);
#endif

#ifdef ENABLE_OOT3D_PRESENTATION
        poll_song_cursor();
#endif
        mem->pad.current_state.hex = state.hex;
        mem->pad.index = next_pad_index;
        next_pad_index = (next_pad_index + 1) % mem->pad.entries.size();

        // Get the previous Pad state
        u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
        PadState old_state = mem->pad.entries[last_entry_index].current_state;

        // Compute bitmask with 1s for bits different from the old state
        PadState changed = {{(state.hex ^ old_state.hex)}};

        // Get the current Pad entry
        PadDataEntry& pad_entry = mem->pad.entries[mem->pad.index];

        // Update entry properties
        pad_entry.current_state.hex = state.hex;
        pad_entry.delta_additions.hex = changed.hex & state.hex;
        pad_entry.delta_removals.hex = changed.hex & old_state.hex;
        pad_entry.circle_pad_x = circle_pad_x;
        pad_entry.circle_pad_y = circle_pad_y;
#ifdef ENABLE_MM3D_PRESENTATION
        MM3D::InputLatency::Hid(state.hex,changed.hex);
#endif

        // If we just updated index 0, provide a new timestamp
        if (mem->pad.index == 0) {
            mem->pad.index_reset_ticks_previous = mem->pad.index_reset_ticks;
            mem->pad.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
        }

        mem->touch.index = next_touch_index;
        next_touch_index = (next_touch_index + 1) % mem->touch.entries.size();

        // Get the current touch entry
        TouchDataEntry& touch_entry = mem->touch.entries[mem->touch.index];
        bool pressed = (data.pad & LIBCTRU_TOUCH_KEY) != 0;

        touch_entry.x = static_cast<u16>(data.touch_x);
        touch_entry.y = static_cast<u16>(data.touch_y);
        touch_entry.valid.Assign(pressed ? 1 : 0);

#ifdef ENABLE_OOT3D_PRESENTATION
        if(OOT3D::PauseMenu::menu_input.HiddenTouch(touch_entry.x,touch_entry.y))touch_entry.valid.Assign(0);
        if(menu_touch) {touch_entry.x=OOT3D::PauseMenu::menu_input.touch_x;touch_entry.y=220;touch_entry.valid.Assign(1);}
#endif
        system.Movie().HandleTouchStatus(touch_entry);
#ifdef ENABLE_MM3D_PRESENTATION
        if(MM3D::ClassicSaving::ResumeNoticeArmed() || MM3D::FileSelectInput::Blocked()) {
            touch_entry.x=0;touch_entry.y=0;touch_entry.valid.Assign(0);
        }
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
        touch_entry.x=0;touch_entry.y=0;touch_entry.valid.Assign(0);
#endif
    } else {
        state.a.Assign(buttons[A - BUTTON_HID_BEGIN]->GetStatus());
        state.b.Assign(buttons[B - BUTTON_HID_BEGIN]->GetStatus());
        state.x.Assign(buttons[X - BUTTON_HID_BEGIN]->GetStatus());
        state.y.Assign(buttons[Y - BUTTON_HID_BEGIN]->GetStatus());
        state.right.Assign(buttons[Right - BUTTON_HID_BEGIN]->GetStatus());
        state.left.Assign(buttons[Left - BUTTON_HID_BEGIN]->GetStatus());
        state.up.Assign(buttons[Up - BUTTON_HID_BEGIN]->GetStatus());
        state.down.Assign(buttons[Down - BUTTON_HID_BEGIN]->GetStatus());
        state.l.Assign(buttons[L - BUTTON_HID_BEGIN]->GetStatus());
        state.r.Assign(buttons[R - BUTTON_HID_BEGIN]->GetStatus());
        state.start.Assign(buttons[Start - BUTTON_HID_BEGIN]->GetStatus());
        state.select.Assign(buttons[Select - BUTTON_HID_BEGIN]->GetStatus());
        state.debug.Assign(buttons[Debug - BUTTON_HID_BEGIN]->GetStatus());
        state.gpio14.Assign(buttons[Gpio14 - BUTTON_HID_BEGIN]->GetStatus());

        // Get current circle pad position and update circle pad direction
        float circle_pad_x_f, circle_pad_y_f;
        std::tie(circle_pad_x_f, circle_pad_y_f) = circle_pad->GetStatus();

        // xperia64: 0x9A seems to be the calibrated limit of the circle pad
        // Verified by using Input Redirector with very large-value digital inputs
        // on the circle pad and calibrating using the system settings application
        constexpr int MAX_CIRCLEPAD_POS = 0x9A; // Max value for a circle pad position

        // These are rounded rather than truncated on actual hardware
        s16 circle_pad_new_x = static_cast<s16>(std::roundf(circle_pad_x_f * MAX_CIRCLEPAD_POS));
        s16 circle_pad_new_y = static_cast<s16>(std::roundf(circle_pad_y_f * MAX_CIRCLEPAD_POS));
        s16 circle_pad_x = (circle_pad_new_x +
                            std::accumulate(circle_pad_old_x.begin(), circle_pad_old_x.end(), 0)) /
                           CIRCLE_PAD_AVERAGING;
        s16 circle_pad_y = (circle_pad_new_y +
                            std::accumulate(circle_pad_old_y.begin(), circle_pad_old_y.end(), 0)) /
                           CIRCLE_PAD_AVERAGING;
        circle_pad_old_x.erase(circle_pad_old_x.begin());
        circle_pad_old_x.push_back(circle_pad_new_x);
        circle_pad_old_y.erase(circle_pad_old_y.begin());
        circle_pad_old_y.push_back(circle_pad_new_y);

#ifdef ENABLE_OOT3D_PRESENTATION
        menu_input((oot_zl->GetStatus()?1u:0u)|(oot_zr->GetStatus()?2u:0u),circle_pad_x,circle_pad_y);
#endif
        system.Movie().HandlePadAndCircleStatus(state, circle_pad_x, circle_pad_y);
#ifdef ENABLE_MM3D_PRESENTATION
        MM3D::OcarinaInput::Filter(state.hex);
        MM3D::ClassicSaving::FilterArrivalPad(state.hex,circle_pad_x,circle_pad_y);
        MM3D::FileSelectInput::Filter(state.hex,circle_pad_x,circle_pad_y);
#endif

        const DirectionState direction = GetStickDirectionState(circle_pad_x, circle_pad_y);
        state.circle_up.Assign(direction.up);
        state.circle_down.Assign(direction.down);
        state.circle_left.Assign(direction.left);
        state.circle_right.Assign(direction.right);

#ifdef ENABLE_OOT3D_PRESENTATION
        poll_song_cursor();
#endif
        mem->pad.current_state.hex = state.hex;
        mem->pad.index = next_pad_index;
        next_pad_index = (next_pad_index + 1) % mem->pad.entries.size();

        // Get the previous Pad state
        u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
        PadState old_state = mem->pad.entries[last_entry_index].current_state;

        // Compute bitmask with 1s for bits different from the old state
        PadState changed = {{(state.hex ^ old_state.hex)}};

        // Get the current Pad entry
        PadDataEntry& pad_entry = mem->pad.entries[mem->pad.index];

        // Update entry properties
        pad_entry.current_state.hex = state.hex;
        pad_entry.delta_additions.hex = changed.hex & state.hex;
        pad_entry.delta_removals.hex = changed.hex & old_state.hex;
        pad_entry.circle_pad_x = circle_pad_x;
        pad_entry.circle_pad_y = circle_pad_y;
#ifdef ENABLE_MM3D_PRESENTATION
        MM3D::InputLatency::Hid(state.hex,changed.hex);
#endif

        // If we just updated index 0, provide a new timestamp
        if (mem->pad.index == 0) {
            mem->pad.index_reset_ticks_previous = mem->pad.index_reset_ticks;
            mem->pad.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
        }

        mem->touch.index = next_touch_index;
        next_touch_index = (next_touch_index + 1) % mem->touch.entries.size();

        // Get the current touch entry
        TouchDataEntry& touch_entry = mem->touch.entries[mem->touch.index];
        bool pressed = false;
        float x, y;
        std::tie(x, y, pressed) = touch_device->GetStatus();
        if (!pressed && touch_btn_device) {
            std::tie(x, y, pressed) = touch_btn_device->GetStatus();
        }
        if (!pressed && controller_touch_device) {
            std::tie(x, y, pressed) = controller_touch_device->GetStatus();
        }
        touch_entry.x = static_cast<u16>(x * Core::kScreenBottomWidth);
        touch_entry.y = static_cast<u16>(y * Core::kScreenBottomHeight);
        touch_entry.valid.Assign(pressed ? 1 : 0);

#ifdef ENABLE_OOT3D_PRESENTATION
        if(OOT3D::PauseMenu::menu_input.HiddenTouch(touch_entry.x,touch_entry.y))touch_entry.valid.Assign(0);
        if(menu_touch) {touch_entry.x=OOT3D::PauseMenu::menu_input.touch_x;touch_entry.y=220;touch_entry.valid.Assign(1);}
#endif
        system.Movie().HandleTouchStatus(touch_entry);
#ifdef ENABLE_MM3D_PRESENTATION
        if(MM3D::ClassicSaving::ResumeNoticeArmed() || MM3D::FileSelectInput::Blocked()) {
            touch_entry.x=0;touch_entry.y=0;touch_entry.valid.Assign(0);
        }
#endif
#ifdef ENABLE_MM3D_GAME_FOLDER
        touch_entry.x=0;touch_entry.y=0;touch_entry.valid.Assign(0);
#endif
    }

#ifdef ENABLE_OOT3D_PRESENTATION
    // Both developer and player builds use buttons for game interaction. Block
    // mouse, touchscreen, touchpad and mapped touch input at the guest boundary,
    // including movie playback. Keep the synthetic press used by pause controls.
    auto& game_touch = mem->touch.entries[mem->touch.index];
    game_touch.x = menu_touch ? OOT3D::PauseMenu::menu_input.touch_x : 0;
    game_touch.y = menu_touch ? 220 : 0;
    game_touch.valid.Assign(menu_touch ? 1 : 0);
#endif

    // TODO(bunnei): We're not doing anything with offset 0xA8 + 0x18 of HID SharedMemory, which
    // supposedly is "Touch-screen entry, which contains the raw coordinate data prior to being
    // converted to pixel coordinates." (http://3dbrew.org/wiki/HID_Shared_Memory#Offset_0xA8).

    // If we just updated index 0, provide a new timestamp
    if (mem->touch.index == 0) {
        mem->touch.index_reset_ticks_previous = mem->touch.index_reset_ticks;
        mem->touch.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    // Signal both handles when there's an update to Pad or touch
    event_pad_or_touch_1->Signal();
    event_pad_or_touch_2->Signal();

    // TODO(xperia64): How the 3D Slider is updated by the HID module needs to be RE'd
    // and possibly moved to its own Core::Timing event.
    mem->pad.sliderstate_3d = (Settings::values.factor_3d.GetValue() / 100.0f);
    system.Kernel().GetSharedPageHandler().Set3DSlider(Settings::values.factor_3d.GetValue() /
                                                       100.0f);

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(pad_update_ticks - cycles_late, pad_update_event);
}

void Module::UpdateAccelerometerCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->accelerometer.index = next_accelerometer_index;
    next_accelerometer_index = (next_accelerometer_index + 1) % mem->accelerometer.entries.size();

    AccelerometerDataEntry& accelerometer_entry =
        mem->accelerometer.entries[mem->accelerometer.index];

    if (artic_controller.get() && artic_controller->IsReady()) {
        ArticBaseController::ControllerData data = artic_controller->GetControllerData();

        accelerometer_entry.x = data.accel_x;
        accelerometer_entry.y = data.accel_y;
        accelerometer_entry.z = data.accel_z;
    } else {
        Common::Vec3<float> accel;
        std::tie(accel, std::ignore) = motion_device->GetStatus();
        accel *= accelerometer_coef;
        // TODO(wwylele): do a time stretch like the one in UpdateGyroscopeCallback
        // The time stretch formula should be like
        // stretched_vector = (raw_vector - gravity) * stretch_ratio + gravity

        accelerometer_entry.x = static_cast<s16>(accel.x);
        accelerometer_entry.y = static_cast<s16>(accel.y);
        accelerometer_entry.z = static_cast<s16>(accel.z);
    }

    system.Movie().HandleAccelerometerStatus(accelerometer_entry);

    // Make up "raw" entry
    // TODO(wwylele):
    // From hardware testing, the raw_entry values are approximately, but not exactly, as twice as
    // corresponding entries (or with a minus sign). It may caused by system calibration to the
    // accelerometer. Figure out how it works, or, if no game reads raw_entry, the following three
    // lines can be removed and leave raw_entry unimplemented.
    mem->accelerometer.raw_entry.x = -2 * accelerometer_entry.x;
    mem->accelerometer.raw_entry.z = 2 * accelerometer_entry.y;
    mem->accelerometer.raw_entry.y = -2 * accelerometer_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->accelerometer.index == 0) {
        mem->accelerometer.index_reset_ticks_previous = mem->accelerometer.index_reset_ticks;
        mem->accelerometer.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    event_accelerometer->Signal();

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(accelerometer_update_ticks - cycles_late,
                                      accelerometer_update_event);
}

void Module::UpdateGyroscopeCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->gyroscope.index = next_gyroscope_index;
    next_gyroscope_index = (next_gyroscope_index + 1) % mem->gyroscope.entries.size();

    GyroscopeDataEntry& gyroscope_entry = mem->gyroscope.entries[mem->gyroscope.index];

    if (artic_controller.get() && artic_controller->IsReady()) {
        ArticBaseController::ControllerData data = artic_controller->GetControllerData();

        gyroscope_entry.x = data.gyro_x;
        gyroscope_entry.y = data.gyro_y;
        gyroscope_entry.z = data.gyro_z;
    } else {
        Common::Vec3<float> gyro;
        std::tie(std::ignore, gyro) = motion_device->GetStatus();
        double stretch = system.perf_stats->GetLastFrameTimeScale();
        gyro *= gyroscope_coef * static_cast<float>(stretch);
        gyroscope_entry.x = static_cast<s16>(gyro.x);
        gyroscope_entry.y = static_cast<s16>(gyro.y);
        gyroscope_entry.z = static_cast<s16>(gyro.z);
    }

    system.Movie().HandleGyroscopeStatus(gyroscope_entry);

    // Make up "raw" entry
    mem->gyroscope.raw_entry.x = gyroscope_entry.x;
    mem->gyroscope.raw_entry.z = -gyroscope_entry.y;
    mem->gyroscope.raw_entry.y = gyroscope_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->gyroscope.index == 0) {
        mem->gyroscope.index_reset_ticks_previous = mem->gyroscope.index_reset_ticks;
        mem->gyroscope.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    event_gyroscope->Signal();

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(gyroscope_update_ticks - cycles_late, gyroscope_update_event);
}

void Module::Interface::GetIPCHandles(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 7);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(hid->shared_mem, hid->event_pad_or_touch_1, hid->event_pad_or_touch_2,
                       hid->event_accelerometer, hid->event_gyroscope, hid->event_debug_pad);
}

void Module::Interface::EnableAccelerometer(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        auto req = artic_client->NewRequest("HIDUSER_EnableAccelerometer");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
        } else {
            rb.Push(Result{static_cast<u32>(resp->GetMethodResult())});
        }
    } else {
        rb.Push(ResultSuccess);
    }

    ++hid->enable_accelerometer_count;

    // Schedules the accelerometer update event if the accelerometer was just enabled
    if (hid->enable_accelerometer_count == 1) {
        hid->system.CoreTiming().ScheduleEvent(accelerometer_update_ticks,
                                               hid->accelerometer_update_event);
    }

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::DisableAccelerometer(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        auto req = artic_client->NewRequest("HIDUSER_DisableAccelerometer");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
        } else {
            rb.Push(Result{static_cast<u32>(resp->GetMethodResult())});
        }
    } else {
        rb.Push(ResultSuccess);
    }

    --hid->enable_accelerometer_count;

    // Unschedules the accelerometer update event if the accelerometer was just disabled
    if (hid->enable_accelerometer_count == 0) {
        hid->system.CoreTiming().UnscheduleEvent(hid->accelerometer_update_event, 0);
    }

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::EnableGyroscopeLow(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        auto req = artic_client->NewRequest("HIDUSER_EnableGyroscope");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
        } else {
            rb.Push(Result{static_cast<u32>(resp->GetMethodResult())});
        }
    } else {
        rb.Push(ResultSuccess);
    }

    ++hid->enable_gyroscope_count;

    // Schedules the gyroscope update event if the gyroscope was just enabled
    if (hid->enable_gyroscope_count == 1) {
        hid->system.CoreTiming().ScheduleEvent(gyroscope_update_ticks, hid->gyroscope_update_event);
    }

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::DisableGyroscopeLow(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        auto req = artic_client->NewRequest("HIDUSER_DisableGyroscope");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
        } else {
            rb.Push(Result{static_cast<u32>(resp->GetMethodResult())});
        }
    } else {
        rb.Push(ResultSuccess);
    }

    --hid->enable_gyroscope_count;

    // Unschedules the gyroscope update event if the gyroscope was just disabled
    if (hid->enable_gyroscope_count == 0) {
        hid->system.CoreTiming().UnscheduleEvent(hid->gyroscope_update_event, 0);
    }

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::GetGyroscopeLowRawToDpsCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        auto req = artic_client->NewRequest("HIDUSER_GetGyroRawToDpsCoef");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
            rb.Push(0.f);
            return;
        }

        Result res = Result{static_cast<u32>(resp->GetMethodResult())};
        if (res.IsError()) {
            rb.Push(res);
            rb.Push(0.f);
            return;
        }

        auto coef = resp->GetResponseFloat(0);
        if (!coef.has_value()) {
            rb.Push(ResultUnknown);
            rb.Push(0.f);
            return;
        }

        rb.Push(res);
        rb.Push(*coef);
    } else {
        rb.Push(ResultSuccess);
        rb.Push(gyroscope_coef);
    }
}

void Module::Interface::GetGyroscopeLowCalibrateParam(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(6, 0);

    auto& artic_client = GetModule()->artic_client;
    if (artic_client.get()) {
        GyroscopeCalibrateParam param;

        auto req = artic_client->NewRequest("HIDUSER_GetGyroCalibrateParam");

        auto resp = artic_client->Send(req);

        if (!resp.has_value()) {
            rb.Push(ResultUnknown);
            rb.PushRaw(param);
            return;
        }

        Result res = Result{static_cast<u32>(resp->GetMethodResult())};
        if (res.IsError()) {
            rb.Push(res);
            rb.PushRaw(param);
            return;
        }

        auto param_buf = resp->GetResponseBuffer(0);
        if (!param_buf.has_value() || param_buf->second != sizeof(param)) {
            rb.Push(ResultUnknown);
            rb.PushRaw(param);
            return;
        }
        memcpy(&param, param_buf->first, sizeof(param));

        rb.Push(res);
        rb.PushRaw(param);
    } else {
        rb.Push(ResultSuccess);

        const s16 param_unit = 6700; // an approximate value taken from hw
        GyroscopeCalibrateParam param = {
            {0, param_unit, -param_unit},
            {0, param_unit, -param_unit},
            {0, param_unit, -param_unit},
        };
        rb.PushRaw(param);

        LOG_WARNING(Service_HID, "(STUBBED) called");
    }
}

void Module::Interface::GetSoundVolume(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const u8 volume = static_cast<u8>(0x3F * Settings::values.volume.GetValue());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(volume);
}

Module::Interface::Interface(std::shared_ptr<Module> hid, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), hid(std::move(hid)) {}

std::shared_ptr<Module> Module::Interface::GetModule() const {
    return hid;
}

Module::Module(Core::System& system) : system(system) {
    using namespace Kernel;

    shared_mem =
        system.Kernel()
            .CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                MemoryPermission::Read, 0, MemoryRegion::BASE, "HID:SharedMemory")
            .Unwrap();

    // Create event handles
    event_pad_or_touch_1 = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventPadOrTouch1");
    event_pad_or_touch_2 = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventPadOrTouch2");
    event_accelerometer = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventAccelerometer");
    event_gyroscope = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventGyroscope");
    event_debug_pad = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventDebugPad");

    // Register update callbacks
    Core::Timing& timing = system.CoreTiming();
    pad_update_event = timing.RegisterEvent("HID::UpdatePadCallback",
                                            [this](std::uintptr_t user_data, s64 cycles_late) {
                                                UpdatePadCallback(user_data, cycles_late);
                                            });
    accelerometer_update_event = timing.RegisterEvent(
        "HID::UpdateAccelerometerCallback", [this](std::uintptr_t user_data, s64 cycles_late) {
            UpdateAccelerometerCallback(user_data, cycles_late);
        });
    gyroscope_update_event = timing.RegisterEvent(
        "HID::UpdateGyroscopeCallback", [this](std::uintptr_t user_data, s64 cycles_late) {
            UpdateGyroscopeCallback(user_data, cycles_late);
        });

    timing.ScheduleEvent(pad_update_ticks, pad_update_event);
}

void Module::UseArticClient(const std::shared_ptr<Network::ArticBase::Client>& client) {
    artic_client = client;
    artic_controller = std::make_shared<ArticBaseController>(client);
    if (!artic_controller->IsCreated()) {
        artic_controller.reset();
    } else {
        auto ir_user = system.ServiceManager().GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user.get()) {
            ir_user->UseArticController(artic_controller);
        }

        auto ir_rst = system.ServiceManager().GetService<Service::IR::IR_RST>("ir:rst");
        if (ir_rst.get()) {
            ir_rst->UseArticController(artic_controller);
        }
    }
}

void Module::ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

const PadState& Module::GetState() const {
    return state;
}

std::shared_ptr<Module> GetModule(Core::System& system) {
    auto hid = system.ServiceManager().GetService<Service::HID::Module::Interface>("hid:USER");
    if (!hid)
        return nullptr;
    return hid->GetModule();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto hid = std::make_shared<Module>(system);
    std::make_shared<User>(hid)->InstallAsService(service_manager);
    std::make_shared<Spvr>(hid)->InstallAsService(service_manager);
}

} // namespace Service::HID
