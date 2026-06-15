#pragma once

#include "vulkan/VulkanPipelineBuilder.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace vr {

// Lightweight struct passed to recording helpers; the owning renderer fills this
// each frame from its member variables before dispatching.
struct RecordState {
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    // Render passes
    VkRenderPass shadowPass = VK_NULL_HANDLE;
    VkRenderPass gbufferPass = VK_NULL_HANDLE;
    VkRenderPass ssaoPass = VK_NULL_HANDLE;
    VkRenderPass forwardPass = VK_NULL_HANDLE;

    // Framebuffers
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer gbufferFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer ssaoFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer ssaoBlurFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer forwardFramebuffer = VK_NULL_HANDLE;  // per-swapchain-image

    // Pipeline layouts
    VkPipelineLayout forwardLayout = VK_NULL_HANDLE;
    VkPipelineLayout v4Layout = VK_NULL_HANDLE;
    VkPipelineLayout v5Layout = VK_NULL_HANDLE;

    // Pipelines
    VkPipeline shadowPipeline = VK_NULL_HANDLE;
    VkPipeline gbufferPipeline = VK_NULL_HANDLE;
    VkPipeline instancedGbufferPipeline = VK_NULL_HANDLE;
    VkPipeline ssaoPipeline = VK_NULL_HANDLE;
    VkPipeline ssaoBlurPipeline = VK_NULL_HANDLE;
    VkPipeline v4ComposePipeline = VK_NULL_HANDLE;
    VkPipeline v5ComputePipeline = VK_NULL_HANDLE;
    VkPipeline skyPipeline = VK_NULL_HANDLE;
    VkPipeline forwardPipeline = VK_NULL_HANDLE;

    // Descriptor sets
    VkDescriptorSet v4DescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet v5DescriptorSet = VK_NULL_HANDLE;      // current frame's set
    VkDescriptorSet fallbackDescriptorSet = VK_NULL_HANDLE; // descriptorSets_.front() or VK_NULL_HANDLE

    // Geometry
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    VkBuffer sphereVertexBuffer = VK_NULL_HANDLE;
    VkBuffer sphereInstanceBuffer = VK_NULL_HANDLE;
    std::uint32_t sphereVertexCount = 0;
    std::uint32_t sphereInstanceCount = 0;
    const std::vector<GpuPreviewGeometry::Batch>* batches = nullptr;

    // GBuffer images (for V5 compute barriers)
    std::array<VkImage, 3> gbufferImages{};

    // V5 history
    VkImage v5HistoryReadImage = VK_NULL_HANDLE;
    VkImage v5HistoryWriteImage = VK_NULL_HANDLE;
    bool v5HistoryReadInitialized = false;
    bool v5HistoryWriteInitialized = false;

    // Swapchain
    VkImage swapchainImage = VK_NULL_HANDLE;
    bool swapchainImageInitialized = false;

    // Viewport
    VkExtent2D extent{};

    // Frame counter
    std::uint32_t frameIndex = 0;

    // Profile flags
    bool enableShadows = false;
    bool enableV4 = false;
    bool enableV5 = false;

    // Material descriptor sets (indexed by batch materialIndex)
    const std::vector<VkDescriptorSet>* materialDescriptorSets = nullptr;
};

// Shared: fill the G-buffer with scene geometry (used by both V4 and V5 paths).
void recordGBufferFill(const RecordState& state);

// Shared: record the shadow atlas pass.
void recordShadowPass(const RecordState& state);

// Profile-specific command recording entry points.
void recordV5RayTracing(const RecordState& state);
void recordV4Deferred(const RecordState& state);
void recordV2V3Forward(const RecordState& state);

} // namespace vr
