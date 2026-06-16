#pragma once

#include "vulkan/VulkanRenderPassBuilder.hpp"

namespace vr {

struct Pipelines {
    VkPipelineLayout forwardLayout = VK_NULL_HANDLE;
    VkPipelineLayout v4ComposeLayout = VK_NULL_HANDLE;
    VkPipelineLayout v5RayTracingLayout = VK_NULL_HANDLE;
    VkPipeline forward = VK_NULL_HANDLE;
    VkPipeline sky = VK_NULL_HANDLE;
    VkPipeline shadow = VK_NULL_HANDLE;
    VkPipeline gbuffer = VK_NULL_HANDLE;
    VkPipeline gbufferInstanced = VK_NULL_HANDLE;
    VkPipeline ssao = VK_NULL_HANDLE;
    VkPipeline ssaoBlur = VK_NULL_HANDLE;
    VkPipeline v4Compose = VK_NULL_HANDLE;
    VkPipeline v5Compute = VK_NULL_HANDLE;
    VkPipeline v5Denoise = VK_NULL_HANDLE;
    VkPipeline v5Downsample = VK_NULL_HANDLE;
};

Pipelines createPipelines(
    VkDevice device,
    VkSampleCountFlagBits msaaSamples,
    const RenderPasses& passes,
    VkDescriptorSetLayout forwardDescriptorLayout,
    VkDescriptorSetLayout v4DescriptorLayout,
    VkDescriptorSetLayout v5DescriptorLayout,
    bool enableV4,
    bool enableV5
);

} // namespace vr
