// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_remote_names.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
namespace Vulkan {
namespace {
constexpr std::array<vk::DescriptorSetLayoutBinding,1> bindings{{
    {0,vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment}}};
constexpr auto vertex_source=R"(#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 color;
layout(location=0) out vec2 texcoord;
layout(location=1) out vec4 tint;
void main(){gl_Position=position;texcoord=uv;tint=color;}
)";
constexpr auto fragment_source=R"(#version 450
layout(set=0,binding=0) uniform sampler2D font;
layout(push_constant) uniform Params {vec4 depth;} params;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 output_color;
void main(){
    float alpha=texture(font,texcoord).a*tint.a;
    if(alpha<0.0039)discard;
    float depth=-gl_FragCoord.z*params.depth.x+params.depth.y;
    if(params.depth.z>0.5)depth/=gl_FragCoord.w;
    gl_FragDepth=clamp(depth,0.0,1.0);
    output_color=vec4(tint.rgb,alpha);
}
)";
}
RemoteNameRenderer::RemoteNameRenderer(const Instance& instance_,Scheduler& scheduler_,RenderManager& manager_,
    DescriptorUpdateQueue& updates_,TextureRuntime& runtime_)
    :instance(instance_),scheduler(scheduler_),manager(manager_),updates(updates_),runtime(runtime_),
     device(instance.GetDevice()),descriptors(instance,scheduler.GetMasterSemaphore(),bindings,16) {
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eFragment,0,16};
    layout=device.createPipelineLayoutUnique({.setLayoutCount=1,.pSetLayouts=&descriptors.Layout(),
        .pushConstantRangeCount=1,.pPushConstantRanges=&push});
    vertex_shader=vk::UniqueShaderModule(Compile(vertex_source,vk::ShaderStageFlagBits::eVertex,device),device);
    fragment_shader=vk::UniqueShaderModule(Compile(fragment_source,vk::ShaderStageFlagBits::eFragment,device),device);
    sampler=device.createSamplerUnique({.magFilter=vk::Filter::eLinear,.minFilter=vk::Filter::eLinear,
        .mipmapMode=vk::SamplerMipmapMode::eNearest,.addressModeU=vk::SamplerAddressMode::eClampToEdge,
        .addressModeV=vk::SamplerAddressMode::eClampToEdge,.addressModeW=vk::SamplerAddressMode::eClampToEdge,
        .maxLod=0});
    font=OOT3D::RemoteNames::MakeFont();
    if(font.pixels.empty())return;
    VideoCore::SurfaceParams params;
    params.width=params.stride=font.width;params.height=font.height;
    params.pixel_format=VideoCore::PixelFormat::RGBA8;params.UpdateParams();
    texture=std::make_unique<Surface>(runtime,params);
    const auto staging=runtime.FindStaging(u32(font.pixels.size()),true);
    std::copy(font.pixels.begin(),font.pixels.end(),staging.mapped.begin());
    texture->Upload({staging.offset,staging.size,params.GetRect(),0},staging);
    font.pixels.clear();font.pixels.shrink_to_fit();
}
RemoteNameRenderer::~RemoteNameRenderer()=default;
vk::Pipeline RemoteNameRenderer::Pipeline(VideoCore::PixelFormat color,VideoCore::PixelFormat depth,vk::CompareOp compare) {
    const auto key=std::tuple{color,depth,compare};
    if(auto it=pipelines.find(key);it!=pipelines.end())return *it->second;
    using V=OOT3D::RemoteNames::Vertex;
    const vk::VertexInputBindingDescription binding{0,sizeof(V),vk::VertexInputRate::eVertex};
    const std::array<vk::VertexInputAttributeDescription,3> attributes{{
        {0,0,vk::Format::eR32G32B32A32Sfloat,offsetof(V,position)},
        {1,0,vk::Format::eR32G32Sfloat,offsetof(V,uv)},
        {2,0,vk::Format::eR32G32B32A32Sfloat,offsetof(V,color)}}};
    const vk::PipelineVertexInputStateCreateInfo vertex{.vertexBindingDescriptionCount=1,.pVertexBindingDescriptions=&binding,
        .vertexAttributeDescriptionCount=3,.pVertexAttributeDescriptions=attributes.data()};
    const vk::PipelineInputAssemblyStateCreateInfo assembly{.topology=vk::PrimitiveTopology::eTriangleList};
    const vk::PipelineViewportStateCreateInfo viewport{.viewportCount=1,.scissorCount=1};
    const vk::PipelineRasterizationStateCreateInfo raster{.polygonMode=vk::PolygonMode::eFill,.cullMode=vk::CullModeFlagBits::eNone,.lineWidth=1};
    const vk::PipelineMultisampleStateCreateInfo msaa{.rasterizationSamples=vk::SampleCountFlagBits::e1};
    const vk::PipelineDepthStencilStateCreateInfo ds{.depthTestEnable=true,.depthWriteEnable=false,.depthCompareOp=compare};
    const vk::PipelineColorBlendAttachmentState blend{
        .blendEnable=true,.srcColorBlendFactor=vk::BlendFactor::eSrcAlpha,.dstColorBlendFactor=vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp=vk::BlendOp::eAdd,.srcAlphaBlendFactor=vk::BlendFactor::eOne,.dstAlphaBlendFactor=vk::BlendFactor::eOneMinusSrcAlpha,
        .alphaBlendOp=vk::BlendOp::eAdd,.colorWriteMask=vk::ColorComponentFlagBits::eR|vk::ColorComponentFlagBits::eG|vk::ColorComponentFlagBits::eB|vk::ColorComponentFlagBits::eA};
    const vk::PipelineColorBlendStateCreateInfo color_blend{.attachmentCount=1,.pAttachments=&blend};
    const std::array dynamic{vk::DynamicState::eViewport,vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamics{.dynamicStateCount=2,.pDynamicStates=dynamic.data()};
    const std::array<vk::PipelineShaderStageCreateInfo,2> shaders{{
        {.stage=vk::ShaderStageFlagBits::eVertex,.module=*vertex_shader,.pName="main"},
        {.stage=vk::ShaderStageFlagBits::eFragment,.module=*fragment_shader,.pName="main"}}};
    const vk::GraphicsPipelineCreateInfo info{.stageCount=2,.pStages=shaders.data(),.pVertexInputState=&vertex,
        .pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,.pMultisampleState=&msaa,
        .pDepthStencilState=&ds,.pColorBlendState=&color_blend,.pDynamicState=&dynamics,.layout=*layout,
        .renderPass=manager.GetRenderpass(color,depth,false)};
    auto result=device.createGraphicsPipelineUnique({},info);
    if(result.result!=vk::Result::eSuccess)return {};
    const auto pipeline=*result.value;pipelines.emplace(key,std::move(result.value));return pipeline;
}
RemoteNameRenderer::Draw RemoteNameRenderer::Prepare(StreamBuffer& buffer,const OOT3D::RemoteNames::Scene& scene,
    float fraction,VideoCore::PixelFormat color,VideoCore::PixelFormat depth,vk::CompareOp compare) {
    using namespace OOT3D::RemoteNames;
    if(!texture || depth==VideoCore::PixelFormat::Invalid)return {};
    auto labels=World(scene.camera,fraction,scene.inverse_view);
    // Translucent text does not write depth: draw far names before near ones.
    std::sort(labels.anchors.begin(),labels.anchors.end(),[&](const Anchor& a,const Anchor& b){
        const auto distance=[&](const Anchor& v){double d=0;for(unsigned c=0;c<3;++c)d+=std::pow(v.position[c]-labels.billboard[c][3],2);return d;};
        return distance(a)>distance(b);
    });
    const auto vertices=Geometry(font,labels,scene,GetLayout());
    if(vertices.empty())return {};
    Draw out;out.pipeline=Pipeline(color,depth,compare);if(!out.pipeline)return {};
    const auto bytes=u32(vertices.size()*sizeof(Vertex));
    const auto [data,offset,invalidated]=buffer.Map(bytes,alignof(Vertex));
    std::memcpy(data,vertices.data(),bytes);buffer.Commit(bytes);
    out.offset=offset;out.count=u32(vertices.size());
    out.descriptor=descriptors.Commit();
    updates.AddImageSampler(out.descriptor,0,0,texture->ImageView(),*sampler);
    out.depth={scene.depth_scale,scene.depth_offset,scene.wbuffer?1.f:0.f,0};
    return out;
}
void RemoteNameRenderer::Render(const Draw& draw,vk::Buffer buffer,vk::Viewport viewport,vk::Rect2D scissor) {
    if(!draw.count)return;
    scheduler.Record([draw,buffer,viewport,scissor,layout=*layout](vk::CommandBuffer command){
        command.bindPipeline(vk::PipelineBindPoint::eGraphics,draw.pipeline);
        command.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,layout,0,draw.descriptor,{});
        command.pushConstants(layout,vk::ShaderStageFlagBits::eFragment,0,16,draw.depth.data());
        command.setViewport(0,viewport);command.setScissor(0,scissor);
        command.bindVertexBuffers(0,buffer,draw.offset);command.draw(draw.count,1,0,0);
    });
    scheduler.MakeDirty(StateFlags::Pipeline|StateFlags::DescriptorSets);
}
}
