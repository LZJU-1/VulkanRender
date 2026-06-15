#pragma once

#include "vulkan/VulkanPass.hpp"
#include "vulkan/VulkanTypes.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace vr {

struct RayTracingPassContext {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;  // current ping-pong set
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::array<VkImage, 3> gbufferImages{};
    std::uint32_t frameIndex = 0;
    bool historyReadInitialized = false;
    bool historyWriteInitialized = false;
    bool swapchainImageInitialized = false;
};

class RayTracingPass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    void setContext(const RayTracingPassContext& ctx) { ctx_ = ctx; }

    VkPipeline pipeline() const { return pipeline_; }
    const AccelerationStructureResource& blas() const { return blas_; }
    const AccelerationStructureResource& tlas() const { return tlas_; }
    const std::array<TextureResource, 2>& historyTargets() const { return historyTargets_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    AccelerationStructureResource blas_{};
    AccelerationStructureResource tlas_{};
    std::array<TextureResource, 2> historyTargets_{};
    RayTracingPassContext ctx_;
};

} // namespace vr
