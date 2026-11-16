// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include "common/polyfill_thread.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

VK_DEFINE_HANDLE(VmaAllocation)

namespace Frontend {
class EmuWindow;
}

#ifdef ENABLE_MM3D_PRESENTATION
#include "mm3d/camera_midpoint.h"
#include "mm3d/input_latency.h"
namespace MM3D { class Overlay; }
#endif

#ifdef ENABLE_OOT3D_PRESENTATION
#include "oot3d/interpolation_state.h"
namespace OOT3D { class Overlay; }
#endif

namespace Vulkan {

class Instance;
class Swapchain;
class Scheduler;
class RenderManager;

struct Frame {
#ifdef ENABLE_OOT3D_PRESENTATION
    u64 oot_rumble_frame{}, oot_rumble_epoch{};
    bool oot_rumble_native{};
    int oot_fps=60;
#endif
#ifdef ENABLE_MM3D_PRESENTATION
    MM3D::InputLatency::Frame input_timing;
#endif
#if defined(ENABLE_MM3D_PRESENTATION) || defined(ENABLE_OOT3D_PRESENTATION)
    bool interpolated_pair{};
#endif
    u32 width;
    u32 height;
    VmaAllocation allocation;
    vk::Framebuffer framebuffer;
    vk::Image image;
    vk::ImageView image_view;
    vk::Semaphore render_ready;
    vk::Fence present_done;
    vk::CommandBuffer cmdbuf;
};

class PresentWindow final {
public:
    explicit PresentWindow(Frontend::EmuWindow& emu_window, const Instance& instance,
                           Scheduler& scheduler, bool low_refresh_rate, bool primary_window = false);
    ~PresentWindow();

    /// Waits for all queued frames to finish presenting.
    void WaitPresent();

    /// Drains and joins the presenter before its owner releases GPU resources.
    /// The producer must have stopped and flushed all queued rendering first.
    void StopPresent();

    /// Returns the last used render frame.
    Frame* GetRenderFrame();
#ifdef ENABLE_OOT3D_PRESENTATION
    // Never wait for optional interpolation; preserve a ready native frame.
    Frame* TryGetIntermediateFrame();
#endif

    /// Recreates the render frame to match provided parameters.
    void RecreateFrame(Frame* frame, u32 width, u32 height);

    /// Queues the provided frame for presentation.
    void Present(Frame* frame);

    /// This is called to notify the rendering backend of a surface change
    void NotifySurfaceChanged();

    [[nodiscard]] vk::RenderPass Renderpass() const noexcept {
        return present_renderpass;
    }

    u32 ImageCount() const noexcept {
        return swapchain.GetImageCount();
    }

    vk::Format GetSurfaceFormat() const noexcept {
        return swapchain.GetSurfaceFormat().format;
    }

private:
#ifdef ENABLE_OOT3D_PRESENTATION
    // Caller owns free_mutex. Only remove frames whose GPU fence has completed.
    Frame* TakeReadyFrame(bool reserve_native);
#endif
    bool HasOverlay() const;
    bool NeedsIdleRedraw() const;
    void PresentThread(std::stop_token token);

    void CopyToSwapchain(Frame* frame, bool repeat = false);

    vk::RenderPass CreateRenderpass();

private:
    Frontend::EmuWindow& emu_window;
    const Instance& instance;
    Scheduler& scheduler;
    bool low_refresh_rate;
    vk::SurfaceKHR surface;
    vk::SurfaceKHR next_surface{};
    Swapchain swapchain;
    vk::CommandPool command_pool;
    vk::Queue graphics_queue;
    vk::RenderPass present_renderpass;
    std::vector<Frame> swap_chain;
    std::queue<Frame*> free_queue;
    std::queue<Frame*> present_queue;
    std::condition_variable free_cv;
    std::condition_variable recreate_surface_cv;
    std::condition_variable_any frame_cv;
    std::mutex swapchain_mutex;
    std::mutex recreate_surface_mutex;
    std::mutex queue_mutex;
    std::mutex free_mutex;
    std::jthread present_thread;
#ifdef ENABLE_MM3D_PRESENTATION
    std::unique_ptr<MM3D::Overlay> mm3d_overlay;
    MM3D::CameraMidpoint::PresentationPacer midpoint_pacer;
#endif
#if defined(ENABLE_MM3D_PRESENTATION) || defined(ENABLE_OOT3D_PRESENTATION)
    // Owned by the presentation thread; never put back in the renderer's free pool
    // until replaced. The background image excludes the developer overlay.
    Frame* overlay_retained{};
    Frame overlay_background{};
#endif
#ifdef ENABLE_OOT3D_PRESENTATION
    std::unique_ptr<OOT3D::Overlay> oot3d_overlay;
    OOT3D::Interpolation::PresentationPacer oot_midpoint_pacer;
    bool overlay_was_visible = false;
#endif
    bool vsync_enabled{};
    bool blit_supported;
    bool use_present_thread{true};
    void* last_render_surface{};
};

} // namespace Vulkan
