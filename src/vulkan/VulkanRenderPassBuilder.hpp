#pragma once

#include "vulkan/VulkanLoader.hpp"

namespace vr {

struct RenderPasses {
    VkRenderPass forward = VK_NULL_HANDLE;
    VkRenderPass shadow = VK_NULL_HANDLE;
    VkRenderPass gbuffer = VK_NULL_HANDLE;
    VkRenderPass ssao = VK_NULL_HANDLE;
};

RenderPasses createRenderPasses(
    VkDevice device,
    VkFormat swapchainFormat,
    VkSampleCountFlagBits msaaSamples,
    bool enableGBuffer,
    bool enableSsao,
    VkSampleCountFlagBits gbufferSamples = VK_SAMPLE_COUNT_1_BIT
);

} // namespace vr
