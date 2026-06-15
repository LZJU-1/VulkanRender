#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdint>

namespace vr {

// Consistent interface for every rendering feature (shadow, gbuffer, ssao, etc.).
// Each pass encapsulates its own pipeline, descriptors, framebuffer, and
// command recording.  Shared resources (vertex buffers, uniforms, swapchain
// images) are accessed through VulkanResources and VulkanDescriptors.
class VulkanPass {
public:
    virtual ~VulkanPass() = default;

    // One-time setup: create pipelines, descriptor sets, framebuffers.
    virtual void create(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkExtent2D extent, VkFormat swapchainFormat) = 0;

    // Per-frame command recording.
    // frameIndex is the monotonic frame counter; swapchainImageIndex is the
    // index returned by vkAcquireNextImageKHR.
    virtual void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                        std::uint32_t swapchainImageIndex) = 0;

    // Release all Vulkan resources owned by this pass.
    virtual void destroy() = 0;
};

} // namespace vr
