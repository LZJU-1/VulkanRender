#pragma once

#include "vulkan/VulkanPass.hpp"
#include "vulkan/VulkanTypes.hpp"

namespace vr {

struct SSAOPassContext {
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    bool enabled = true;
};

class SSAOPass : public VulkanPass {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkExtent2D extent, VkFormat swapchainFormat) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t swapchainImageIndex) override;
    void destroy() override;

    void setContext(const SSAOPassContext& ctx) { ctx_ = ctx; }

    VkRenderPass renderPass() const { return renderPass_; }
    VkFramebuffer rawFramebuffer() const { return rawFramebuffer_; }
    VkFramebuffer blurFramebuffer() const { return blurFramebuffer_; }
    VkPipeline rawPipeline() const { return rawPipeline_; }
    VkPipeline blurPipeline() const { return blurPipeline_; }
    const TextureResource& rawTarget() const { return rawTarget_; }
    const TextureResource& blurTarget() const { return blurTarget_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer rawFramebuffer_ = VK_NULL_HANDLE;
    VkFramebuffer blurFramebuffer_ = VK_NULL_HANDLE;
    VkPipeline rawPipeline_ = VK_NULL_HANDLE;
    VkPipeline blurPipeline_ = VK_NULL_HANDLE;
    TextureResource rawTarget_{};
    TextureResource blurTarget_{};
    SSAOPassContext ctx_;
};

} // namespace vr
