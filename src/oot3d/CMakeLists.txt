add_library(oot3d_hd_font STATIC hd_font_artwork.cpp hd_system_font.cpp hd_rupee_font.cpp hd_boss_timer_font.cpp)
target_include_directories(oot3d_hd_font PUBLIC "${PROJECT_SOURCE_DIR}/src")

add_executable(oot3d_atomic_shared_ptr_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/atomic-shared-ptr-check.cpp)
target_include_directories(oot3d_atomic_shared_ptr_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(oot3d_atomic_shared_ptr_check PRIVATE Threads::Threads)
target_compile_options(oot3d_atomic_shared_ptr_check PRIVATE -UNDEBUG)

add_executable(oot3d_release_defaults_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/release-defaults-check.cpp release_defaults.qrc
    world_map_outline.cpp map_cutout.cpp)
set_target_properties(oot3d_release_defaults_check PROPERTIES AUTORCC ON)
target_include_directories(oot3d_release_defaults_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(oot3d_release_defaults_check PRIVATE Qt6::Core Qt6::Gui citra_common)
if (APPLE)
    target_link_libraries(oot3d_release_defaults_check PRIVATE
        "-framework Cocoa" "-framework AVFoundation" "-framework CoreGraphics")
endif()

add_executable(oot3d_player_controls_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/player-controls-check.cpp player_mode.cpp)
target_include_directories(oot3d_player_controls_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(oot3d_player_controls_check PRIVATE citra_common)
if (APPLE)
    target_link_libraries(oot3d_player_controls_check PRIVATE
        "-framework Cocoa" "-framework AVFoundation" "-framework CoreGraphics")
endif()

add_executable(oot3d_hd_font_check EXCLUDE_FROM_ALL ../../tools/oot3d/hd-font-check.cpp)
target_link_libraries(oot3d_hd_font_check PRIVATE oot3d_hd_font dynarmic)
target_compile_options(oot3d_hd_font_check PRIVATE -UNDEBUG)

add_executable(oot3d_hd_rupee_check EXCLUDE_FROM_ALL ../../tools/oot3d/hd-rupee-check.cpp)
target_link_libraries(oot3d_hd_rupee_check PRIVATE oot3d_hd_font)
target_compile_options(oot3d_hd_rupee_check PRIVATE -UNDEBUG)

add_executable(oot3d_hd_boss_timer_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/hd-boss-timer-check.cpp ../video_core/texture/etc1.cpp)
target_link_libraries(oot3d_hd_boss_timer_check PRIVATE oot3d_hd_font Boost::boost)
target_compile_options(oot3d_hd_boss_timer_check PRIVATE -UNDEBUG)

add_executable(oot3d_hd_numbers_config_check EXCLUDE_FROM_ALL ../../tools/oot3d/hd-numbers-config-check.cpp)
target_link_libraries(oot3d_hd_numbers_config_check PRIVATE oot3d_hd_font Qt6::Core)
target_compile_options(oot3d_hd_numbers_config_check PRIVATE -UNDEBUG)

add_executable(oot3d_remote_names_check EXCLUDE_FROM_ALL ../../tools/oot3d/remote-names-check.cpp)
target_include_directories(oot3d_remote_names_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_options(oot3d_remote_names_check PRIVATE -UNDEBUG)

add_library(oot3d_randomizer_data STATIC
    randomizer/data.cpp randomizer/service.cpp randomizer/presets.cpp randomizer/oot_randomizer_assets.qrc)
set_target_properties(oot3d_randomizer_data PROPERTIES AUTORCC ON)
target_include_directories(oot3d_randomizer_data PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(oot3d_randomizer_data PUBLIC Qt6::Core PRIVATE citra_common)
add_subdirectory("${PROJECT_SOURCE_DIR}/tools/oot3d/randomizer" "${CMAKE_CURRENT_BINARY_DIR}/seed-generator")

# OoT3D presentation and editor: no MM3D game hooks or settings are linked.
set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/externals/mm3d-imgui")
add_library(oot3d_overlay STATIC
    player_mode.cpp
    randomizer/ui.cpp randomizer/icons.cpp multiplayer_settings.cpp
    actor_capture.cpp
    surface_capture.cpp
    map_cutout.cpp map_cutout_editor.cpp world_map_outline.cpp
    overlay.cpp overlay.h input.h settings.h title_layout.h title_hooks.h hud_layout.h hud_hooks.h
    user_hud.h hud_defaults.h user_camera.h user_camera_config.h
    texture_picker.cpp texture_picker.h texture_inspector.cpp texture_inspector.h inspection_clock.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(oot3d_overlay SYSTEM PUBLIC ${IMGUI_DIR})
target_compile_definitions(oot3d_overlay PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(oot3d_overlay PUBLIC vulkan-headers PRIVATE citra_common Qt6::Core oot3d_randomizer_data)

# Exercise the shared DekUI vertex replay engine in the OoT build as well.
add_executable(oot3d_cpu_replay_check EXCLUDE_FROM_ALL ../mm3d/cpu_vertex_replay_tests.cpp)
target_link_libraries(oot3d_cpu_replay_check PRIVATE oot3d_overlay video_core
    nihstro-headers tsl::robin_map Qt6::Core ${PLATFORM_LIBRARIES})

add_executable(oot3d_dialogue_native_check EXCLUDE_FROM_ALL ../../tools/oot3d/dialogue-native-check.cpp)
target_link_libraries(oot3d_dialogue_native_check PRIVATE dynarmic)

add_executable(oot3d_shop_controls_check EXCLUDE_FROM_ALL ../../tools/oot3d/shop-controls-check.cpp)
target_link_libraries(oot3d_shop_controls_check PRIVATE dynarmic)
target_compile_options(oot3d_shop_controls_check PRIVATE -UNDEBUG)

add_executable(oot3d_rumble_check EXCLUDE_FROM_ALL ../../tools/oot3d/rumble-check.cpp)
target_link_libraries(oot3d_rumble_check PRIVATE dynarmic)
target_compile_options(oot3d_rumble_check PRIVATE -UNDEBUG)

add_executable(oot3d_qol_check EXCLUDE_FROM_ALL ../../tools/oot3d/qol-check.cpp)
target_link_libraries(oot3d_qol_check PRIVATE dynarmic)
target_compile_options(oot3d_qol_check PRIVATE -UNDEBUG)

add_executable(oot3d_sun_readback_check EXCLUDE_FROM_ALL ../../tools/oot3d/sun-readback-check.cpp)
target_link_libraries(oot3d_sun_readback_check PRIVATE dynarmic)
target_compile_options(oot3d_sun_readback_check PRIVATE -UNDEBUG)

add_executable(oot3d_sun_depth_gpu_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/sun-depth-gpu-check.cpp ../mm3d/visibility_depth_fixture.cpp)
target_link_libraries(oot3d_sun_depth_gpu_check PRIVATE oot3d_overlay video_core
    tsl::robin_map Qt6::Core ${PLATFORM_LIBRARIES})
target_compile_definitions(oot3d_sun_depth_gpu_check PRIVATE QT_NO_KEYWORDS)

if (ENABLE_VULKAN)
    add_executable(oot3d_bloom_gpu_check EXCLUDE_FROM_ALL ../../tools/oot3d/bloom-gpu-check.cpp)
    target_link_libraries(oot3d_bloom_gpu_check PRIVATE oot3d_overlay video_core tsl::robin_map Qt6::Core ${PLATFORM_LIBRARIES})
    target_compile_definitions(oot3d_bloom_gpu_check PRIVATE QT_NO_KEYWORDS)
endif()

add_executable(oot3d_ocarina_check EXCLUDE_FROM_ALL ../../tools/oot3d/ocarina-check.cpp)
target_link_libraries(oot3d_ocarina_check PRIVATE dynarmic)
target_compile_options(oot3d_ocarina_check PRIVATE -UNDEBUG)

add_executable(oot3d_save_quit_check EXCLUDE_FROM_ALL ../../tools/oot3d/save-quit-check.cpp)
target_link_libraries(oot3d_save_quit_check PRIVATE dynarmic)
target_compile_options(oot3d_save_quit_check PRIVATE -UNDEBUG)

add_executable(oot3d_pause_framerate_check EXCLUDE_FROM_ALL ../../tools/oot3d/pause-framerate-check.cpp)
add_executable(oot3d_arrow_cycle_check EXCLUDE_FROM_ALL ../../tools/oot3d/arrow-cycle-check.cpp)
target_link_libraries(oot3d_arrow_cycle_check PRIVATE dynarmic)
target_compile_options(oot3d_arrow_cycle_check PRIVATE -UNDEBUG)
target_link_libraries(oot3d_pause_framerate_check PRIVATE dynarmic)
target_compile_options(oot3d_pause_framerate_check PRIVATE -UNDEBUG)

add_executable(oot3d_pause_description_check EXCLUDE_FROM_ALL ../../tools/oot3d/pause-description-check.cpp)
target_link_libraries(oot3d_pause_description_check PRIVATE dynarmic)
target_compile_options(oot3d_pause_description_check PRIVATE -UNDEBUG)

add_executable(oot3d_dpad_check EXCLUDE_FROM_ALL ../../tools/oot3d/dpad-check.cpp)
target_link_libraries(oot3d_dpad_check PRIVATE dynarmic)
target_compile_options(oot3d_dpad_check PRIVATE -UNDEBUG)

add_executable(oot3d_aim_native_check EXCLUDE_FROM_ALL ../../tools/oot3d/aim-native-check.cpp)
target_link_libraries(oot3d_aim_native_check PRIVATE dynarmic)
target_compile_options(oot3d_aim_native_check PRIVATE -UNDEBUG)

add_executable(oot3d_boss_challenge_check EXCLUDE_FROM_ALL ../../tools/oot3d/boss-challenge-check.cpp)
target_link_libraries(oot3d_boss_challenge_check PRIVATE dynarmic oot3d_hd_font)
target_compile_options(oot3d_boss_challenge_check PRIVATE -UNDEBUG)

add_executable(oot3d_actor_capture_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/actor-capture-check.cpp actor_capture.cpp)
target_link_libraries(oot3d_actor_capture_check PRIVATE Qt6::Core)
target_compile_options(oot3d_actor_capture_check PRIVATE -UNDEBUG)

add_executable(oot3d_surface_capture_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/surface-capture-check.cpp surface_capture.cpp)
target_link_libraries(oot3d_surface_capture_check PRIVATE Qt6::Core)
target_compile_options(oot3d_surface_capture_check PRIVATE -UNDEBUG)

add_executable(oot3d_surface_effects_check EXCLUDE_FROM_ALL ../../tools/oot3d/surface-effects-check.cpp)
target_link_libraries(oot3d_surface_effects_check PRIVATE Qt6::Core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_surface_effects_check PRIVATE -UNDEBUG)
add_executable(oot3d_captured_effects_check EXCLUDE_FROM_ALL ../../tools/oot3d/captured-effects-check.cpp)
target_link_libraries(oot3d_captured_effects_check PRIVATE Qt6::Core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_captured_effects_check PRIVATE -UNDEBUG)

add_executable(oot3d_song_visual_check EXCLUDE_FROM_ALL ../../tools/oot3d/song-visual-check.cpp)
target_link_libraries(oot3d_song_visual_check PRIVATE Qt6::Core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_song_visual_check PRIVATE -UNDEBUG)

add_executable(oot3d_actor_draw_distance_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/actor-draw-distance-check.cpp)
target_link_libraries(oot3d_actor_draw_distance_check PRIVATE dynarmic)
target_compile_options(oot3d_actor_draw_distance_check PRIVATE -UNDEBUG)

add_executable(oot3d_camera_clearance_check EXCLUDE_FROM_ALL ../../tools/oot3d/camera-clearance-check.cpp)
target_link_libraries(oot3d_camera_clearance_check PRIVATE dynarmic)
target_compile_options(oot3d_camera_clearance_check PRIVATE -UNDEBUG)

add_executable(oot3d_hud_fade_check EXCLUDE_FROM_ALL ../../tools/oot3d/hud-fade-check.cpp)
target_link_libraries(oot3d_hud_fade_check PRIVATE dynarmic)
target_compile_options(oot3d_hud_fade_check PRIVATE -UNDEBUG)

add_executable(oot3d_guest_ui_lifetime_check EXCLUDE_FROM_ALL ../../tools/oot3d/guest-ui-lifetime-check.cpp)
target_link_libraries(oot3d_guest_ui_lifetime_check PRIVATE dynarmic)
target_compile_options(oot3d_guest_ui_lifetime_check PRIVATE -UNDEBUG)

add_executable(oot3d_graphics_memory_check EXCLUDE_FROM_ALL ../../tools/oot3d/graphics-memory-check.cpp)
target_link_libraries(oot3d_graphics_memory_check PRIVATE dynarmic)
target_compile_options(oot3d_graphics_memory_check PRIVATE -UNDEBUG)

add_executable(oot3d_save_wait_diagnostics_check EXCLUDE_FROM_ALL ../../tools/oot3d/save-wait-diagnostics-check.cpp)
target_compile_options(oot3d_save_wait_diagnostics_check PRIVATE -UNDEBUG)

add_executable(oot3d_file_particles_check EXCLUDE_FROM_ALL ../../tools/oot3d/file-particles-check.cpp)
target_compile_options(oot3d_file_particles_check PRIVATE -UNDEBUG)

add_executable(oot3d_file_copy_check EXCLUDE_FROM_ALL ../../tools/oot3d/file-copy-check.cpp)
target_compile_options(oot3d_file_copy_check PRIVATE -UNDEBUG)

add_executable(oot3d_file_overwrite_check EXCLUDE_FROM_ALL ../../tools/oot3d/file-overwrite-check.cpp)
target_compile_options(oot3d_file_overwrite_check PRIVATE -UNDEBUG)

add_executable(oot3d_master_quest_check EXCLUDE_FROM_ALL ../../tools/oot3d/master-quest-check.cpp)
target_compile_options(oot3d_master_quest_check PRIVATE -UNDEBUG)

add_executable(oot3d_room_join_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/room-join-check.cpp ../citra_qt/multiplayer/oot_room_join.cpp)
target_link_libraries(oot3d_room_join_check PRIVATE network citra_common
    Qt6::Core Qt6::Network Qt6::Concurrent ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_room_join_check PRIVATE -UNDEBUG)

add_executable(oot3d_uds_rejoin_check EXCLUDE_FROM_ALL ../../tools/oot3d/uds-rejoin-check.cpp)
target_link_libraries(oot3d_uds_rejoin_check PRIVATE citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_uds_rejoin_check PRIVATE -UNDEBUG)

add_executable(oot3d_file_presentation_check EXCLUDE_FROM_ALL ../../tools/oot3d/file-presentation-check.cpp)
target_compile_options(oot3d_file_presentation_check PRIVATE -UNDEBUG)

add_executable(oot3d_randomizer_check EXCLUDE_FROM_ALL ../../tools/oot3d/randomizer/runtime_check.cpp)
target_link_libraries(oot3d_randomizer_check PRIVATE dynarmic oot3d_randomizer_data citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_randomizer_check PRIVATE -UNDEBUG)

add_executable(oot3d_randomizer_memory_check EXCLUDE_FROM_ALL ../../tools/oot3d/randomizer/memory_check.cpp)
target_link_libraries(oot3d_randomizer_memory_check PRIVATE dynarmic)
target_compile_options(oot3d_randomizer_memory_check PRIVATE -UNDEBUG)

add_executable(oot3d_randomizer_resource_check EXCLUDE_FROM_ALL ../../tools/oot3d/randomizer/resource_diagnostics_check.cpp)
target_compile_options(oot3d_randomizer_resource_check PRIVATE -UNDEBUG)

add_executable(oot3d_name_keyboard_check EXCLUDE_FROM_ALL ../../tools/oot3d/name-keyboard-check.cpp)
target_link_libraries(oot3d_name_keyboard_check PRIVATE dynarmic)
target_compile_options(oot3d_name_keyboard_check PRIVATE -UNDEBUG -O0)

add_executable(oot3d_circle_wipe_check EXCLUDE_FROM_ALL ../../tools/oot3d/circle-wipe-check.cpp)
target_link_libraries(oot3d_circle_wipe_check PRIVATE Qt6::Core video_core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_circle_wipe_check PRIVATE -UNDEBUG)


add_executable(oot3d_interpolation_rate_check EXCLUDE_FROM_ALL ../../tools/oot3d/interpolation-rate-check.cpp)
target_include_directories(oot3d_interpolation_rate_check PRIVATE ..)
target_compile_options(oot3d_interpolation_rate_check PRIVATE -UNDEBUG)

add_executable(oot3d_biri_interpolation_check EXCLUDE_FROM_ALL ../../tools/oot3d/biri-interpolation-check.cpp)
target_link_libraries(oot3d_biri_interpolation_check PRIVATE Qt6::Core video_core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_biri_interpolation_check PRIVATE -UNDEBUG)

add_executable(oot3d_gohma_hit_check EXCLUDE_FROM_ALL ../../tools/oot3d/gohma-hit-check.cpp)
target_link_libraries(oot3d_gohma_hit_check PRIVATE Qt6::Core video_core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_gohma_hit_check PRIVATE -UNDEBUG)
add_executable(oot3d_gohma_death_check EXCLUDE_FROM_ALL ../../tools/oot3d/gohma-death-check.cpp)
target_link_libraries(oot3d_gohma_death_check PRIVATE Qt6::Core video_core dynarmic citra_common vulkan-headers sirit SPIRV-Tools-static ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_gohma_death_check PRIVATE -UNDEBUG)

add_executable(oot3d_syabom_interpolation_check EXCLUDE_FROM_ALL ../../tools/oot3d/syabom-interpolation-check.cpp)
target_link_libraries(oot3d_syabom_interpolation_check PRIVATE Qt6::Core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_syabom_interpolation_check PRIVATE -UNDEBUG)

add_executable(oot3d_kokiri_trail_check EXCLUDE_FROM_ALL ../../tools/oot3d/kokiri-trail-check.cpp)
target_link_libraries(oot3d_kokiri_trail_check PRIVATE Qt6::Core dynarmic citra_common ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_kokiri_trail_check PRIVATE -UNDEBUG)

# Personal HUD composition and persistence checks use temporary settings only.
foreach(check IN ITEMS user-hud hud-layout hud-extras magic-meter ammo-backdrop)
    string(REPLACE "-" "_" target_name "oot3d_${check}_check")
    add_executable(${target_name} EXCLUDE_FROM_ALL ../../tools/oot3d/${check}-check.cpp)
    target_link_libraries(${target_name} PRIVATE oot3d_hd_font Qt6::Core)
    target_compile_options(${target_name} PRIVATE -UNDEBUG)
endforeach()

add_executable(oot3d_user_camera_check EXCLUDE_FROM_ALL ../../tools/oot3d/user-camera-check.cpp)
target_include_directories(oot3d_user_camera_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(oot3d_user_camera_check PRIVATE Qt6::Core)
target_compile_options(oot3d_user_camera_check PRIVATE -UNDEBUG)

add_executable(oot3d_randomizer_presets_check EXCLUDE_FROM_ALL ../../tools/oot3d/randomizer-presets-check.cpp)
target_link_libraries(oot3d_randomizer_presets_check PRIVATE oot3d_randomizer_data citra_common Qt6::Core ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_randomizer_presets_check PRIVATE -UNDEBUG)

# Only the frontend policy and overlay differ; gameplay/defaults are identical.
if (ENABLE_OOT3D_PLAYER_MODE)
    set_source_files_properties(player_mode.cpp overlay.cpp PROPERTIES
        COMPILE_DEFINITIONS ENABLE_OOT3D_PLAYER_MODE)
else()
    set_source_files_properties(player_mode.cpp PROPERTIES
        COMPILE_DEFINITIONS "OOT3D_DEFAULTS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/defaults\"")
endif()

add_executable(oot3d_fast_loading_check EXCLUDE_FROM_ALL
    ../../tools/oot3d/fast-loading-check.cpp
    ../core/file_sys/ivfc_archive.cpp ../core/file_sys/layered_fs.cpp
    ../core/file_sys/delay_generator.cpp ../core/file_sys/patch.cpp)
target_link_libraries(oot3d_fast_loading_check PRIVATE citra_common oot3d_randomizer_data
    Boost::serialization ${PLATFORM_LIBRARIES})
target_compile_options(oot3d_fast_loading_check PRIVATE -UNDEBUG)
