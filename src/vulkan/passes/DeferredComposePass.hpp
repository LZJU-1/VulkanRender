#pragma once

#include "vulkan/VulkanPass.hpp"
#include <cstdint>

namespace vr {

struct DeferredComposePassContext {
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::uint32_t v4DebugMode = 0;
};

class DeferredComposePass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    void setContext(const DeferredComposePassContext& ctx) { ctx_ = ctx; }

    VkPipeline pipeline() const { return pipeline_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    DeferredComposePassContext ctx_;
};

} // namespace vr
