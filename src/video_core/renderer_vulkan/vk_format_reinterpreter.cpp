// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_vulkan/vk_format_reinterpreter.h"
#include "video_core/renderer_vulkan/vk_shader.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

namespace Vulkan {

D24S8toRGBA8::D24S8toRGBA8(const Instance& instance, TaskScheduler& scheduler,
                           TextureRuntime& runtime)
    : FormatReinterpreterBase{instance, scheduler, runtime}, device{instance.GetDevice()} {
    constexpr std::string_view cs_source = R"(
#version 450 core
#extension GL_EXT_samplerless_texture_functions : require
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform highp texture2D depth;
layout(set = 0, binding = 1) uniform lowp utexture2D stencil;
layout(set = 0, binding = 2, rgba8) uniform highp writeonly image2D color;

layout(push_constant, std140) uniform ComputeInfo {
    mediump ivec2 src_offset;
};

void main() {
    ivec2 tex_coord = src_offset + ivec2(gl_GlobalInvocationID.xy);

    highp uint depth_val =
        uint(texelFetch(depth, tex_coord, 0).x * (exp2(32.0) - 1.0));
    lowp uint stencil_val = texelFetch(stencil, tex_coord, 0).x;
    highp uvec4 components =
        uvec4(stencil_val, (uvec3(depth_val) >> uvec3(24u, 16u, 8u)) & 0x000000FFu);
    imageStore(color, tex_coord, vec4(components) / (exp2(8.0) - 1.0));
}

)";
    compute_shader =
        Compile(cs_source, vk::ShaderStageFlagBits::eCompute, device, ShaderOptimization::High);

    const std::array compute_layout_bindings = {
        vk::DescriptorSetLayoutBinding{.binding = 0,
                                       .descriptorType = vk::DescriptorType::eSampledImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{.binding = 1,
                                       .descriptorType = vk::DescriptorType::eSampledImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{.binding = 2,
                                       .descriptorType = vk::DescriptorType::eStorageImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute}};

    const vk::DescriptorSetLayoutCreateInfo compute_layout_info = {
        .bindingCount = static_cast<u32>(compute_layout_bindings.size()),
        .pBindings = compute_layout_bindings.data()};

    descriptor_layout = device.createDescriptorSetLayout(compute_layout_info);

    const std::array update_template_entries = {
        vk::DescriptorUpdateTemplateEntry{.dstBinding = 0,
                                          .dstArrayElement = 0,
                                          .descriptorCount = 1,
                                          .descriptorType = vk::DescriptorType::eSampledImage,
                                          .offset = 0,
                                          .stride = sizeof(vk::DescriptorImageInfo)},
        vk::DescriptorUpdateTemplateEntry{.dstBinding = 1,
                                          .dstArrayElement = 0,
                                          .descriptorCount = 1,
                                          .descriptorType = vk::DescriptorType::eSampledImage,
                                          .offset = sizeof(vk::DescriptorImageInfo),
                                          .stride = 0},
        vk::DescriptorUpdateTemplateEntry{.dstBinding = 2,
                                          .dstArrayElement = 0,
                                          .descriptorCount = 1,
                                          .descriptorType = vk::DescriptorType::eStorageImage,
                                          .offset = 2 * sizeof(vk::DescriptorImageInfo),
                                          .stride = 0}};

    const vk::DescriptorUpdateTemplateCreateInfo template_info = {
        .descriptorUpdateEntryCount = static_cast<u32>(update_template_entries.size()),
        .pDescriptorUpdateEntries = update_template_entries.data(),
        .templateType = vk::DescriptorUpdateTemplateType::eDescriptorSet,
        .descriptorSetLayout = descriptor_layout};

    update_template = device.createDescriptorUpdateTemplate(template_info);

    const vk::PushConstantRange push_range = {
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(Common::Vec2i),
    };

    const vk::PipelineLayoutCreateInfo layout_info = {.setLayoutCount = 1,
                                                      .pSetLayouts = &descriptor_layout,
                                                      .pushConstantRangeCount = 1,
                                                      .pPushConstantRanges = &push_range};

    compute_pipeline_layout = device.createPipelineLayout(layout_info);

    const vk::PipelineShaderStageCreateInfo compute_stage = {
        .stage = vk::ShaderStageFlagBits::eCompute, .module = compute_shader, .pName = "main"};

    const vk::ComputePipelineCreateInfo compute_info = {.stage = compute_stage,
                                                        .layout = compute_pipeline_layout};

    if (const auto result = device.createComputePipeline({}, compute_info);
        result.result == vk::Result::eSuccess) {
        compute_pipeline = result.value;
    } else {
        LOG_CRITICAL(Render_Vulkan, "D24S8 compute pipeline creation failed!");
        UNREACHABLE();
    }
}

D24S8toRGBA8::~D24S8toRGBA8() {
    device.destroyPipeline(compute_pipeline);
    device.destroyPipelineLayout(compute_pipeline_layout);
    device.destroyDescriptorUpdateTemplate(update_template);
    device.destroyDescriptorSetLayout(descriptor_layout);
    device.destroyShaderModule(compute_shader);
}

void D24S8toRGBA8::Reinterpret(Surface& source, VideoCore::Rect2D src_rect, Surface& dest,
                               VideoCore::Rect2D dst_rect) {
    vk::CommandBuffer command_buffer = scheduler.GetRenderCommandBuffer();
    runtime.Transition(command_buffer, source.alloc, vk::ImageLayout::eDepthStencilReadOnlyOptimal,
                       0, source.alloc.levels);
    runtime.Transition(command_buffer, dest.alloc, vk::ImageLayout::eGeneral, 0, dest.alloc.levels);

    const std::array textures = {
        vk::DescriptorImageInfo{.imageView = source.GetDepthView(),
                                .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal},
        vk::DescriptorImageInfo{.imageView = source.GetStencilView(),
                                .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal},
        vk::DescriptorImageInfo{.imageView = dest.GetImageView(),
                                .imageLayout = vk::ImageLayout::eGeneral}};

    const vk::DescriptorSetAllocateInfo alloc_info = {.descriptorPool =
                                                          scheduler.GetDescriptorPool(),
                                                      .descriptorSetCount = 1,
                                                      .pSetLayouts = &descriptor_layout};

    descriptor_set = device.allocateDescriptorSets(alloc_info)[0];

    device.updateDescriptorSetWithTemplate(descriptor_set, update_template, textures[0]);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_layout, 0,
                                      1, &descriptor_set, 0, nullptr);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);

    const auto src_offset = Common::MakeVec(src_rect.left, src_rect.bottom);
    command_buffer.pushConstants(compute_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                                 sizeof(Common::Vec2i), src_offset.AsArray());

    command_buffer.dispatch(src_rect.GetWidth() / 8, src_rect.GetHeight() / 8, 1);
}

} // namespace Vulkan