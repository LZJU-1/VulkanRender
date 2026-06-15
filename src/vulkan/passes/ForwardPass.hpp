#pragma once

#include "vulkan/VulkanPass.hpp"
#include <cstdint>
#include <vector>

namespace vr {

struct ForwardPassContext {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    VkDescriptorSet fallbackDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    const std::vector<GpuPreviewGeometry::Batch>* batches = nullptr;
    const std::vector<VkDescriptorSet>* materialDescriptorSets = nullptr;
};

class ForwardPass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    void setContext(const ForwardPassContext& ctx) { ctx_ = ctx; }

    VkPipeline skyPipeline() const { return skyPipeline_; }
    VkPipeline meshPipeline() const { return meshPipeline_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipeline skyPipeline_ = VK_NULL_HANDLE;
    VkPipeline meshPipeline_ = VK_NULL_HANDLE;
    ForwardPassContext ctx_;
};

} // namespace vr
