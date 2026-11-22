// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>
#include <limits>
#include <stdexcept>
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
#include "video_core/renderer_vulkan/vk_scene_effects.h"

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
static void CheckBloom(const Vulkan::Instance& instance,Vulkan::Scheduler& scheduler, u32 scale) {
    Vulkan::RenderManager manager(instance,scheduler);
    Vulkan::DescriptorUpdateQueue updates(instance);
    Vulkan::TextureRuntime runtime(instance,scheduler,manager,updates,3);
    scheduler.RegisterOnSubmit([&]{manager.EndRendering();});
    scheduler.RegisterOnDispatch([&]{updates.Flush();});
    {
        Vulkan::SceneEffects effects(instance,scheduler,manager,updates);
        const u32 width = 240 * scale, height = 400 * scale;
        const vk::Rect2D region{{0,0},{width,height}};
        VideoCore::SurfaceParams params;
        params.addr=0x18500000;params.width=params.stride=width;params.height=height;
        params.res_scale=1;params.pixel_format=VideoCore::PixelFormat::RGBA8;params.UpdateParams();
        Vulkan::Surface color(runtime,params);
        params.pixel_format=VideoCore::PixelFormat::D24S8;params.UpdateParams();
        Vulkan::Surface depth(runtime,params);
        for(bool with_depth:{false,true}) {
            Vulkan::Framebuffer target(runtime,{},&color,with_depth?&depth:nullptr);
            const auto paint=[&](float intensity, int stripe_x = 0, bool fxaa = false, bool diagonal = false) {
                const auto begin=[&]{manager.BeginRendering(&target,color.GetScaledRect());};
                const auto rectangle=[&](vk::Rect2D rect,float luminance) {
                    scheduler.Record([rect,luminance](vk::CommandBuffer cmd) {
                        const vk::ClearAttachment clear{vk::ImageAspectFlagBits::eColor,0,
                            vk::ClearValue{vk::ClearColorValue{std::array<float,4>{luminance,luminance,luminance,0.75f}}}};
                        cmd.clearAttachments(clear,vk::ClearRect{rect,0,1});
                    });
                };
                begin();rectangle(region,0.125f);rectangle({{80,100},{40,40}},1.f);
                if (stripe_x) rectangle({{stripe_x,800},{3,400}},1.f);
                // Extend the stair-step past the processing region to check the scissor too.
                if(diagonal)for(int y=0;y<96;++y)rectangle({{y/2,y},{16,1}},1.f);
                const vk::Rect2D area=diagonal?vk::Rect2D{{8,8},{width-16,height-16}}:region;
                const Vulkan::SceneEffects::Options options{fxaa,intensity};
                if(fxaa || intensity>0) {
                    Require(effects.Prepare(color.Image(),target.ImageView(VideoCore::SurfaceType::Color),
                        region.extent,area,options),"prepare scene effects");
                    begin();effects.Composite(target.RenderPass(),area,options);
                }
                rectangle({{74,108},{6,20}},0.5f); // Dim HUD overlaps the glowing edge.
                rectangle({{40,240},{12,12}},1.f); // White HUD on a dark background.
                manager.EndRendering();
                const auto staging=runtime.FindStaging(width*height*4,false);
                color.Download({.buffer_offset=staging.offset,.buffer_size=staging.size,
                    .texture_rect=color.GetRect(),.texture_level=0},staging);
                scheduler.Finish();
                return std::vector<u8>(staging.mapped.begin(),staging.mapped.begin()+width*height*4);
            };
            const auto plain=paint(0),low=paint(.45f),high=paint(1);
            unsigned glow_pixels=0,brighter=0;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const auto i=(y*width+x)*4;
                const bool hud=(x>=74 && x<80 && y>=108 && y<128) || (y>=220 && y<270);
                for(unsigned c=0;c<4;++c) {
                    if(hud)Require(plain[i+c]==high[i+c],"HUD or its background received bloom");
                    if(high[i+c]>plain[i+c])++glow_pixels;
                    if(high[i+c]>low[i+c])++brighter;
                }
            }
            Require(glow_pixels>100 && brighter>100,"bright world edge did not bloom with increasing strength");
            for(float strength:{0.f,.45f}) {
                const auto baseline=paint(strength,0,false,true);
                const auto smoothed=paint(strength,0,true,true);
                unsigned softened=0;
                for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                    const auto i=(y*width+x)*4;
                    const bool outside=x<8 || y<8 || x>=width-8 || y>=height-8;
                    const bool hud=x>=74 && x<80 && y>=108 && y<128;
                    const bool flat=x>=10 && x<30 && y>=280 && y<300;
                    if(outside || hud || flat)
                        Require(std::equal(baseline.begin()+i,baseline.begin()+i+4,smoothed.begin()+i),
                                "FXAA changed pixels outside the scene, flat color, or HUD");
                    Require(baseline[i+3]==smoothed[i+3],"FXAA changed destination alpha");
                    if(y>=20 && y<80 && x>=y/2-1 && x<=y/2+1 &&
                       smoothed[i]!=baseline[i] && smoothed[i]>32 && smoothed[i]<255)++softened;
                }
                Require(softened>30,"FXAA failed to smooth the diagonal edge");
            }
            if (scale == 5 && !with_depth) {
                unsigned min_energy = std::numeric_limits<unsigned>::max(), max_energy = 0;
                for (int x = 500; x < 517; ++x) {
                    const auto pixels = paint(1, x);
                    unsigned energy = 0;
                    for (int dx = -70; dx <= 70; ++dx) {
                        if (dx >= 0 && dx < 3) continue; // Measure only the halo.
                        const auto index = (1000 * width + x + dx) * 4;
                        for (int c = 0; c < 4; ++c)
                            energy += std::max(0, int(pixels[index+c]) - int(plain[index+c]));
                    }
                    min_energy = std::min(min_energy, energy);
                    max_energy = std::max(max_energy, energy);
                }
                std::cout << "Moving highlight halo energy at 5x: " << min_energy << " to " << max_energy << '\n';
                Require(max_energy && min_energy >= max_energy * .90,
                        "moving highlight flickered across the bloom sample grid");
            }
        }
        scheduler.Finish();scheduler.WaitWorker();
    }
    scheduler.RegisterOnSubmit([]{});scheduler.RegisterOnDispatch([]{});
    std::cout<<"FXAA diagonal, flat color, alpha, HUD and subrectangle checks passed. Bloom GPU check passed: scene highlights glow, intensity changes the halo, and HUD pixels and adjacent background match bloom-off on both render passes\n";
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);QTemporaryDir directory;
    FileUtil::SetUserPath(directory.path().toStdString());
    Common::Log::Initialize();Common::Log::Start();
    try {
        HeadlessWindow window;Vulkan::Instance instance(window,0);Vulkan::Scheduler scheduler(instance);
        scheduler.RegisterOnSubmit([]{});scheduler.RegisterOnDispatch([]{});
        CheckBloom(instance,scheduler,1);
        CheckBloom(instance,scheduler,5);
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
