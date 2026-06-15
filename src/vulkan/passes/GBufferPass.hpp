#pragma once

#include "vulkan/VulkanPass.hpp"
#include "vulkan/VulkanTypes.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace vr {

struct GBufferPassContext {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    VkBuffer sphereVertexBuffer = VK_NULL_HANDLE;
    VkBuffer sphereInstanceBuffer = VK_NULL_HANDLE;
    std::uint32_t sphereVertexCount = 0;
    std::uint32_t sphereInstanceCount = 0;
    VkDescriptorSet fallbackDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    const std::vector<GpuPreviewGeometry::Batch>* batches = nullptr;
    const std::vector<VkDescriptorSet>* materialDescriptorSets = nullptr;
};

class GBufferPass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    void setContext(const GBufferPassContext& ctx) { ctx_ = ctx; }

    VkRenderPass renderPass() const { return renderPass_; }
    VkFramebuffer framebuffer() const { return framebuffer_; }
    VkPipeline pipeline() const { return pipeline_; }
    VkPipeline instancedPipeline() const { return instancedPipeline_; }
    const auto& targets() const { return targets_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline instancedPipeline_ = VK_NULL_HANDLE;
    std::array<TextureResource, 3> targets_{};
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    GBufferPassContext ctx_;
};

} // namespace vr
