#pragma once

#include "vulkan/VulkanPass.hpp"
#include <array>
#include <cstdint>

namespace vr {

// Context shared with the renderer — filled each frame before calling record().
struct ShadowPassContext {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    VkDescriptorSet fallbackDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

class ShadowPass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    // Set per-frame context before record().
    void setContext(const ShadowPassContext& ctx) { ctx_ = ctx; }

    VkRenderPass renderPass() const { return renderPass_; }
    VkFramebuffer framebuffer() const { return framebuffer_; }
    VkPipeline pipeline() const { return pipeline_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    ShadowPassContext ctx_;
};

} // namespace vr
