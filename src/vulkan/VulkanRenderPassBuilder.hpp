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
    bool enableSsao
);

} // namespace vr
