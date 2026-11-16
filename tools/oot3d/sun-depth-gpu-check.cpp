// SPDX-License-Identifier: GPL-2.0-or-later
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <QCoreApplication>
#include <QTemporaryDir>
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"
#include "video_core/renderer_vulkan/vk_visibility_depth.h"

namespace MM3D {void RunVisibilityDepthFixture(const Vulkan::Instance&, Vulkan::Scheduler&);}
class HeadlessWindow final : public Frontend::EmuWindow {
public:
    HeadlessWindow() {
#ifdef __APPLE__
        window_info.type=Frontend::WindowSystemType::MacOS;
#elif defined(_WIN32)
        window_info.type=Frontend::WindowSystemType::Windows;
#else
        window_info.type=Frontend::WindowSystemType::X11;
#endif
    }
    void PollEvents() override {}
};
static void Require(bool condition, const char* message) {
    if(!condition)throw std::runtime_error(message);
}
static void CheckPending(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler) {
    Vulkan::RenderManager manager(instance,scheduler);
    Vulkan::DescriptorUpdateQueue updates(instance);
    Vulkan::TextureRuntime runtime(instance,scheduler,manager,updates,3);
    scheduler.RegisterOnSubmit([&]{manager.EndRendering();});
    scheduler.RegisterOnDispatch([&]{updates.Flush();});
    for(const auto size : {std::pair{400u,240u},std::pair{480u,400u}})for(u32 scale:{1u,2u,4u}) {
        VideoCore::SurfaceParams params;
        params.addr=0x185447c0;params.width=params.stride=size.first;params.height=size.second;
        params.res_scale=scale;params.is_tiled=true;params.pixel_format=VideoCore::PixelFormat::D24S8;
        params.UpdateParams();
        Vulkan::Surface surface(runtime,params);
        Vulkan::VisibilityDepthCache cache(instance,scheduler);
        const auto paint=[&](float depth) {
            VideoCore::TextureClear clear{};clear.texture_rect=surface.GetScaledRect();
            clear.value.depth=depth;clear.value.stencil=255;
            runtime.ClearTexture(surface,clear);surface.MarkValid(surface.GetInterval());
        };
        MM3D::VisibilityDepth::Context context{1,1,1,0x100000,0x55};
        u64 now=1'000'000'000;
        bool pending{};
        paint(1.f);
        Require(cache.Read(runtime,surface,surface.addr,context,now,true,&pending)==0 && pending,
            "cold asynchronous sun starts occluded without an exact read");
        scheduler.Finish();
        ++context.frame;++context.batch;now+=33'333'333;
        paint(0.f);
        Require(cache.Read(runtime,surface,surface.addr,context,now,true,&pending)==0xffffffff && !pending,
            "completed previous sky restores visible sun, despite newly occluded depth");
        scheduler.Finish();
        ++context.frame;++context.batch;now+=33'333'333;
        paint(1.f);
        Require(cache.Read(runtime,surface,surface.addr,context,now,true,&pending)==0xff000000 && !pending,
            "completed occluder hides sun despite newly clear depth");
        scheduler.Finish();

        // Hold actual GPU work behind a semaphore. Cold/expired/full-ring paths
        // must all return while the GPU is still blocked, without reading any
        // buffer that the GPU owns. The watchdog prevents a regression hanging.
        Require(instance.IsTimelineSemaphoreSupported(),"timeline semaphore required");
        const auto device=instance.GetDevice();
        const vk::SemaphoreTypeCreateInfo type{.semaphoreType=vk::SemaphoreType::eTimeline};
        auto gate=device.createSemaphoreUnique({.pNext=&type});
        std::atomic<bool> released{};
        const auto release=[&] {if(!released.exchange(true))device.signalSemaphoreKHR({.semaphore=*gate,.value=1});};
        std::jthread watchdog([&] {std::this_thread::sleep_for(std::chrono::milliseconds(250));release();});
        scheduler.Flush(nullptr,*gate);
        cache.Reset();
        for(unsigned frame=0;frame<5;++frame) {
            ++context.frame;++context.batch;now+=33'333'333;paint(float(frame%2));
            for(unsigned offset:{0u,4u,params.size-4}) {
                Require(cache.Read(runtime,surface,surface.addr+offset,context,now,true,&pending)==0 && pending,
                    "pending/full-ring sun reads remain occluded and nonblocking");
            }
        }
        const bool nonblocking=!released.load();release();scheduler.Finish();
        Require(nonblocking,"pending policy must return before GPU gate release");
        // Scene change, elapsed wall time and CPU writes cannot expose old sun.
        ++context.scene;++context.batch;now+=1'000'000'000;
        Require(cache.Read(runtime,surface,surface.addr,context,now,true,&pending)==0 && pending,
            "new scene rejects old visible depth");
        scheduler.Finish();cache.Invalidate(surface.addr,4);++context.batch;
        Require(cache.Read(runtime,surface,surface.addr,context,now+1,true,&pending)==0 && pending,
            "CPU writes discard previous visibility");
        scheduler.Finish();
        std::cout<<"PASS OoT pending policy: "<<size.first<<'x'<<size.second<<" scale="<<scale
            <<"; visible/occluded transitions, blocked GPU, full ring, scene and CPU invalidation\n";
    }
    scheduler.Finish();scheduler.WaitWorker();
    scheduler.RegisterOnSubmit([]{});scheduler.RegisterOnDispatch([]{});
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);QTemporaryDir directory;
    FileUtil::SetUserPath(directory.path().toStdString());
    Common::Log::Initialize();Common::Log::Start();
    try {
        HeadlessWindow window;Vulkan::Instance instance(window,0);Vulkan::Scheduler scheduler(instance);
        scheduler.RegisterOnSubmit([]{});scheduler.RegisterOnDispatch([]{});
        CheckPending(instance,scheduler);
        MM3D::RunVisibilityDepthFixture(instance,scheduler);
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
