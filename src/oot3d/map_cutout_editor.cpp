// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_cutout_editor.h"
#include "oot3d/map_cutout.h"
#include "oot3d/world_map_assets.h"
#include "oot3d/texture_picker.h"
#include "oot3d/pause_menu.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <cstring>

namespace OOT3D {
struct MapCutoutEditor::Impl {
    const Vulkan::Instance& instance;
    std::mutex& submit_mutex;
    std::shared_ptr<const MapFrame::SourcePixels> source;
    std::shared_ptr<Textures::Thumbnail> image;
    vk::Sampler sampler;
    VkDescriptorSet set{};
    MapFrame::Cutout cut;
    bool open=false, preview=false, pixel_grid=true;
    int selected=-1,dragging=-1;
    float zoom=1.5f,feather=8,rounding=8;
    bool world_map=false;
    std::string message;
    std::vector<float> preview_key;
    std::vector<unsigned char> preview_alpha;
    Impl(const Vulkan::Instance& i,std::mutex& m,bool world):instance(i),submit_mutex(m),world_map(world) {
        if(world_map)cut={0,0,320,240,{}};
    }
    ~Impl() {
        if(set)ImGui_ImplVulkan_RemoveTexture(set);
        image.reset();if(sampler)instance.GetDevice().destroySampler(sampler);
    }
    unsigned MemoryType(vk::MemoryRequirements r,vk::MemoryPropertyFlags flags) {
        const auto props=instance.GetPhysicalDevice().getMemoryProperties();
        for(unsigned i=0;i<props.memoryTypeCount;++i)
            if((r.memoryTypeBits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("No memory type for map source preview");
    }
    void Upload() {
        const auto next=world_map?WorldMap::GetSource():MapFrame::GetSource();if(next==source)return;
        std::scoped_lock lock{submit_mutex,instance.PresentQueueMutex()};
        auto device=instance.GetDevice();device.waitIdle();
        if(set)ImGui_ImplVulkan_RemoveTexture(set);set={};image.reset();source=next;
        if(world_map) {
            if(const auto outline=WorldMap::GetOutline()){cut=outline->cut;feather=outline->feather;rounding=outline->rounding;}
        } else {std::scoped_lock cut_lock{MapFrame::cutout_mutex};cut=MapFrame::saved_cutout;}
        selected=dragging=-1;
        if(!source)return;
        constexpr unsigned width=MapFrame::SourceWidth,height=MapFrame::SourceHeight;
        auto staging=device.createBufferUnique({.size=width*height*4,
            .usage=vk::BufferUsageFlagBits::eTransferSrc,.sharingMode=vk::SharingMode::eExclusive});
        auto req=device.getBufferMemoryRequirements(*staging);
        auto memory=device.allocateMemoryUnique({.allocationSize=req.size,
            .memoryTypeIndex=MemoryType(req,vk::MemoryPropertyFlagBits::eHostVisible|vk::MemoryPropertyFlagBits::eHostCoherent)});
        device.bindBufferMemory(*staging,*memory,0);
        auto* data=static_cast<unsigned char*>(device.mapMemory(*memory,0,width*height*4));
        for(unsigned i=0;i<source->size();++i) {
            const auto p=(*source)[i];
            data[4*i]=p>>24;data[4*i+1]=p>>16;data[4*i+2]=p>>8;data[4*i+3]=p;
        }
        device.unmapMemory(*memory);
        image=std::make_shared<Textures::Thumbnail>();image->device=device;
        image->image=device.createImage({.imageType=vk::ImageType::e2D,.format=vk::Format::eR8G8B8A8Unorm,
            .extent={width,height,1},.mipLevels=1,.arrayLayers=1,.samples=vk::SampleCountFlagBits::e1,
            .tiling=vk::ImageTiling::eOptimal,.usage=vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eSampled,
            .sharingMode=vk::SharingMode::eExclusive,.initialLayout=vk::ImageLayout::eUndefined});
        req=device.getImageMemoryRequirements(image->image);
        image->memory=device.allocateMemory({.allocationSize=req.size,.memoryTypeIndex=MemoryType(req,vk::MemoryPropertyFlagBits::eDeviceLocal)});
        device.bindImageMemory(image->image,image->memory,0);
        const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor,0,1,0,1};
        image->view=device.createImageView({.image=image->image,.viewType=vk::ImageViewType::e2D,
            .format=vk::Format::eR8G8B8A8Unorm,.subresourceRange=range});
        auto pool=device.createCommandPoolUnique({.queueFamilyIndex=instance.GetGraphicsQueueFamilyIndex()});
        auto commands=device.allocateCommandBuffersUnique({.commandPool=*pool,.level=vk::CommandBufferLevel::ePrimary,.commandBufferCount=1});
        auto cmd=*commands[0];cmd.begin({.flags=vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        vk::ImageMemoryBarrier barrier{.dstAccessMask=vk::AccessFlagBits::eTransferWrite,
            .oldLayout=vk::ImageLayout::eUndefined,.newLayout=vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=image->image,.subresourceRange=range};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,vk::PipelineStageFlagBits::eTransfer,{}, {}, {},barrier);
        const vk::BufferImageCopy region{.imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={width,height,1}};
        cmd.copyBufferToImage(*staging,image->image,vk::ImageLayout::eTransferDstOptimal,region);
        barrier.srcAccessMask=vk::AccessFlagBits::eTransferWrite;barrier.dstAccessMask=vk::AccessFlagBits::eShaderRead;
        barrier.oldLayout=vk::ImageLayout::eTransferDstOptimal;barrier.newLayout=vk::ImageLayout::eShaderReadOnlyOptimal;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,vk::PipelineStageFlagBits::eFragmentShader,{}, {}, {},barrier);
        cmd.end();
        auto fence=device.createFenceUnique({});
        instance.GetGraphicsQueue().submit(vk::SubmitInfo{.commandBufferCount=1,.pCommandBuffers=&cmd},*fence);
        if(device.waitForFences(*fence,true,UINT64_MAX)!=vk::Result::eSuccess)throw std::runtime_error("Map preview upload failed");
        staging.reset();memory.reset();
        if(!sampler)sampler=device.createSampler({.magFilter=vk::Filter::eNearest,.minFilter=vk::Filter::eNearest,
            .addressModeU=vk::SamplerAddressMode::eClampToEdge,.addressModeV=vk::SamplerAddressMode::eClampToEdge,
            .addressModeW=vk::SamplerAddressMode::eClampToEdge});
        set=ImGui_ImplVulkan_AddTexture(sampler,image->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
};
MapCutoutEditor::MapCutoutEditor(const Vulkan::Instance& i,std::mutex& m,bool world):impl(std::make_unique<Impl>(i,m,world)){}
MapCutoutEditor::~MapCutoutEditor()=default;
void MapCutoutEditor::Draw() {
    auto& s=*impl;
    if(ImGui::Button(s.world_map?"Edit map outline":"Set cutout coordinates"))s.open=true;
    if(!s.open)return;
    ImGui::SetNextWindowSize({850,720},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(s.world_map?"World map outline":"Boss Rush border cutout",&s.open)) {ImGui::End();return;}
    s.Upload();
    if(!s.source) {ImGui::TextWrapped("%s",s.world_map?"Open the overworld map in the pause menu to load its artwork.":"Start the game to load the original boss-card texture.");ImGui::End();return;}
    if(s.world_map) {
        ImGui::TextWrapped("Click around the map to keep. Drag points to refine its outline. The outside is removed and the inside edge fades into the parchment. Apply to save and update the game.");
        ImGui::SliderFloat("Corner rounding",&s.rounding,0,64,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Edge feather",&s.feather,0,64,"%.1f px",ImGuiSliderFlags_AlwaysClamp);
    } else {
    ImGui::TextWrapped("Set the outer rectangle, then click around the inside edge of the border. The closed outline is the area to remove. Drag a point or select it and type its X/Y.");
    ImGui::InputInt("Left",&s.cut.left);ImGui::SameLine();ImGui::TextUnformatted("Source pixels, origin at top left");
    ImGui::InputInt("Top",&s.cut.top);ImGui::InputInt("Right (exclusive)",&s.cut.right);ImGui::InputInt("Bottom (exclusive)",&s.cut.bottom);
    }
    ImGui::SliderFloat("Preview zoom",&s.zoom,.5f,64.f,"%.1fx",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Pixel grid at 8x zoom and above",&s.pixel_grid);
    ImGui::Checkbox(s.world_map?"Preview feathered outline":"Preview removed area",&s.preview);
    if(ImGui::Button("Undo last point") && !s.cut.hole.empty()) {s.cut.hole.pop_back();s.selected=-1;}
    ImGui::SameLine();if(ImGui::Button("Clear points")) {s.cut.hole.clear();s.selected=-1;}
    if(s.selected>=0 && unsigned(s.selected)<s.cut.hole.size()) {
        ImGui::Text("Point %d",s.selected+1);
        ImGui::InputFloat("Point X",&s.cut.hole[s.selected].x,1,10,"%.1f");
        ImGui::InputFloat("Point Y",&s.cut.hole[s.selected].y,1,10,"%.1f");
        if(ImGui::Button("Delete selected point")) {s.cut.hole.erase(s.cut.hole.begin()+s.selected);s.selected=-1;}
    }
    const bool valid=s.world_map?WorldMap::Valid({s.cut,s.feather,s.rounding}):MapFrame::Valid(s.cut);
    ImGui::BeginDisabled(!valid);
    if(ImGui::Button(s.world_map?"Apply outline and save":"Apply cutout and save coordinates")) {
        const bool ok=s.world_map?WorldMap::ApplyOutline({s.cut,s.feather,s.rounding}):MapFrame::ApplyCutout(s.cut);
        s.message=ok?(s.world_map?"Outline saved and applied to the map.":"Cutout applied. Move and scale it as one piece in the border settings."):"Could not save coordinates.";
    }
    ImGui::EndDisabled();
    if(!valid)ImGui::TextWrapped("%s",s.world_map?"Place at least 3 non-collinear points within the map image.":"Choose a valid outer rectangle and at least 3 non-collinear points inside it.");
    ImGui::TextWrapped("%s",s.message.c_str());
    const int canvas_width=s.world_map?int(WorldMap::MapWidth):MapFrame::SourceWidth;
    if(ImGui::BeginChild("Source image",{0,0},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto origin=ImGui::GetCursorScreenPos();
        const ImVec2 size{canvas_width*s.zoom,MapFrame::SourceHeight*s.zoom};
        ImGui::InvisibleButton("Coordinate canvas",size,ImGuiButtonFlags_MouseButtonLeft);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddImage((ImTextureID)s.set,origin,{origin.x+size.x,origin.y+size.y},{0,0},{float(canvas_width)/MapFrame::SourceWidth,1});
        const auto screen=[&](float x,float y){return ImVec2{origin.x+x*s.zoom,origin.y+y*s.zoom};};
        const auto mouse=ImGui::GetIO().MousePos;
        MapFrame::Point point{std::clamp((mouse.x-origin.x)/s.zoom,0.f,float(canvas_width)),
            std::clamp((mouse.y-origin.y)/s.zoom,0.f,float(MapFrame::SourceHeight))};
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("X %.1f   Y %.1f",point.x,point.y);
            if(ImGui::IsMouseClicked(0)) {
                s.selected=-1;float distance=100;
                for(unsigned i=0;i<s.cut.hole.size();++i) {
                    const auto p=screen(s.cut.hole[i].x,s.cut.hole[i].y);
                    const float d=(p.x-mouse.x)*(p.x-mouse.x)+(p.y-mouse.y)*(p.y-mouse.y);
                    if(d<distance){distance=d;s.selected=i;}
                }
                if(s.selected<0 && s.cut.hole.size()<128) {s.cut.hole.push_back(point);s.selected=s.cut.hole.size()-1;}
                s.dragging=s.selected;
            }
        }
        if(s.dragging>=0 && unsigned(s.dragging)<s.cut.hole.size() && ImGui::IsMouseDragging(0))s.cut.hole[s.dragging]=point;
        if(!ImGui::IsMouseDown(0))s.dragging=-1;
        if(s.preview && valid && s.world_map) {
            std::vector<float> key{s.feather,s.rounding};
            for(const auto p:s.cut.hole){key.push_back(p.x);key.push_back(p.y);}
            if(key!=s.preview_key || s.preview_alpha.empty()) {
                const WorldMap::Outline outline{s.cut,s.feather,s.rounding};
                const auto boundary=WorldMap::Boundary(outline);
                s.preview_alpha.resize(canvas_width*MapFrame::SourceHeight);
                for(int y=0;y<MapFrame::SourceHeight;++y)for(int x=0;x<canvas_width;++x)
                    s.preview_alpha[y*canvas_width+x]=int(255*(1-WorldMap::Alpha(boundary,s.feather,x+.5f,y+.5f)));
                s.preview_key=std::move(key);
            }
            // Cache the distance mask while idle and merge equal-alpha runs.
            for(int y=0;y<MapFrame::SourceHeight;++y)for(int x=0;x<canvas_width;) {
                const int alpha=s.preview_alpha[y*canvas_width+x],start=x++;
                while(x<canvas_width && s.preview_alpha[y*canvas_width+x]==alpha)++x;
                if(alpha)draw->AddRectFilled(screen(start,y),screen(x,y+1),IM_COL32(65,45,25,alpha));
            }
        } else if(s.preview && valid) {
            // Scanline runs show the exact pixel mask, including concave parts.
            for(int y=0;y<MapFrame::SourceHeight;++y)for(int x=0;x<MapFrame::SourceWidth;) {
                const auto removed=[&](int xx){return xx<s.cut.left || xx>=s.cut.right || y<s.cut.top || y>=s.cut.bottom || MapFrame::Inside(s.cut.hole,xx+.5f,y+.5f);};
                if(!removed(x)){++x;continue;}
                const int begin=x++;while(x<MapFrame::SourceWidth && removed(x))++x;
                draw->AddRectFilled(screen(begin,y),screen(x,y+1),IM_COL32(30,30,30,255));
            }
        }
        if(s.pixel_grid && s.zoom>=8.f) {
            for(int x=0;x<=canvas_width;++x)
                draw->AddLine(screen(x,0),screen(x,MapFrame::SourceHeight),IM_COL32(0,0,0,100));
            for(int y=0;y<=MapFrame::SourceHeight;++y)
                draw->AddLine(screen(0,y),screen(canvas_width,y),IM_COL32(0,0,0,100));
        }
        if(!s.world_map)draw->AddRect(screen(s.cut.left,s.cut.top),screen(s.cut.right,s.cut.bottom),IM_COL32(0,255,120,255),0,0,2);
        if(s.world_map && valid) {
            const auto boundary=WorldMap::Boundary({s.cut,s.feather,s.rounding});
            for(unsigned i=0;i<boundary.size();++i) {
                const auto a=boundary[i],b=boundary[(i+1)%boundary.size()];
                draw->AddLine(screen(a.x,a.y),screen(b.x,b.y),IM_COL32(255,210,0,255),2);
            }
        }
        for(unsigned i=0;i<s.cut.hole.size();++i) {
            auto a=s.cut.hole[i],b=s.cut.hole[(i+1)%s.cut.hole.size()];
            draw->AddLine(screen(a.x,a.y),screen(b.x,b.y),s.world_map?IM_COL32(255,210,0,65):IM_COL32(255,210,0,255),1);
            draw->AddCircleFilled(screen(a.x,a.y),5,i==unsigned(s.selected)?IM_COL32(255,80,80,255):IM_COL32(255,210,0,255));
            const auto at=screen(a.x,a.y);const auto label=std::to_string(i+1);
            draw->AddText({at.x+6,at.y+4},IM_COL32_WHITE,label.c_str());
        }
    }
    ImGui::EndChild();ImGui::End();
}
}
