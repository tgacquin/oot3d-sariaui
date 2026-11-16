// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/randomizer/icons.h"
#include "oot3d/texture_picker.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace OOT3D::Randomizer {
struct IconAtlas::Impl {
    const Vulkan::Instance& instance;
    std::mutex& submit_mutex;
    std::shared_ptr<const IconPixels> source;
    std::shared_ptr<Textures::Thumbnail> image;
    vk::Sampler sampler;
    VkDescriptorSet set{};
    Impl(const Vulkan::Instance& i, std::mutex& m) : instance(i), submit_mutex(m) {}
    ~Impl() {
        if(set) ImGui_ImplVulkan_RemoveTexture(set);
        image.reset();
        if(sampler) instance.GetDevice().destroySampler(sampler);
    }
    unsigned MemoryType(vk::MemoryRequirements r,vk::MemoryPropertyFlags flags) {
        const auto props=instance.GetPhysicalDevice().getMemoryProperties();
        for(unsigned i=0;i<props.memoryTypeCount;++i)
            if((r.memoryTypeBits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("No memory type for map source preview");
    }
    void Upload() {
        const auto next = icon_pixels.load();
        if(next == source) return;
        std::scoped_lock lock{submit_mutex, instance.PresentQueueMutex()};
        auto device = instance.GetDevice();
        device.waitIdle();
        if(set) ImGui_ImplVulkan_RemoveTexture(set);
        set = {}; image.reset(); source = next;
        if(!source) return;
        constexpr unsigned width = IconAtlasSize, height = IconAtlasSize;
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
        if(device.waitForFences(*fence,true,UINT64_MAX)!=vk::Result::eSuccess)throw std::runtime_error("Randomizer icon upload failed");
        staging.reset();memory.reset();
        if(!sampler)sampler=device.createSampler({.magFilter=vk::Filter::eLinear,.minFilter=vk::Filter::eLinear,
            .addressModeU=vk::SamplerAddressMode::eClampToEdge,.addressModeV=vk::SamplerAddressMode::eClampToEdge,
            .addressModeW=vk::SamplerAddressMode::eClampToEdge});
        set=ImGui_ImplVulkan_AddTexture(sampler,image->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
};
IconAtlas::IconAtlas(const Vulkan::Instance& i, std::mutex& m) : impl(std::make_unique<Impl>(i,m)) {}
IconAtlas::~IconAtlas() = default;
std::uint64_t IconAtlas::Texture() {
    impl->Upload();
    return reinterpret_cast<std::uint64_t>(impl->set);
}
}
