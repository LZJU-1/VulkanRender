#include "platform/VulkanPreviewWindow.hpp"
#include "vulkan/VulkanTypes.hpp"
#include "vulkan/VulkanLoader.hpp"
#include "vulkan/VulkanRenderPassBuilder.hpp"
#include "vulkan/VulkanPipelineBuilder.hpp"
#include "vulkan/VulkanCommandRecorder.hpp"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace vr {
namespace {

class VulkanGpuRenderer {
public:
    VulkanGpuRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height, const GpuPreviewGeometry& geometry, bool enableV3Shadows, bool enableV4Ssao, bool enableV5RayTracing)
        : hwnd_(hwnd), width_(width), height_(height), geometry_(geometry), enableV3Shadows_(enableV3Shadows), enableV4Ssao_(enableV4Ssao), enableV5RayTracing_(enableV5RayTracing) {
        previewLog("VulkanGpuRenderer: loadVulkanLibrary");
        loadVulkanLibrary();
        previewLog("VulkanGpuRenderer: createInstance");
        createInstance();
        previewLog("VulkanGpuRenderer: loadInstanceFunctions");
        loadInstanceFunctions(instance_);
        previewLog("VulkanGpuRenderer: createSurface");
        createSurface();
        previewLog("VulkanGpuRenderer: selectDevice");
        selectDevice();
        previewLog("VulkanGpuRenderer: createDevice");
        createDevice();
        previewLog("VulkanGpuRenderer: loadDeviceFunctions");
        loadDeviceFunctions(device_, enableV5RayTracing_);
        previewLog("VulkanGpuRenderer: fetchDeviceQueues");
        fetchDeviceQueues();
        previewLog("VulkanGpuRenderer: createSwapchain");
        createSwapchain();
        previewLog("VulkanGpuRenderer: createRenderPasses (factory)");
        const bool enableGBuffer = enableV4Ssao_ || enableV5RayTracing_;
        gbufferSamples_ = enableV5RayTracing_ ? msaaSamples_ : VK_SAMPLE_COUNT_1_BIT;
        previewLog("G-buffer samples: " + std::to_string(sampleCountValue(gbufferSamples_)) + "x");
        RenderPasses renderPasses = createRenderPasses(device_, swapchainFormat_, msaaSamples_, enableGBuffer, enableV4Ssao_, gbufferSamples_);
        renderPass_ = renderPasses.forward;
        shadowRenderPass_ = renderPasses.shadow;
        gbufferRenderPass_ = renderPasses.gbuffer;
        ssaoRenderPass_ = renderPasses.ssao;
        previewLog("VulkanGpuRenderer: createDepthResources");
        createDepthResources();
        previewLog("VulkanGpuRenderer: createShadowResources");
        createShadowResources();
        if (enableGBuffer) {
            previewLog("VulkanGpuRenderer: createGBufferResources");
            createGBufferResources();
        }
        if (enableV4Ssao_) {
            previewLog("VulkanGpuRenderer: createSsaoResources");
            createSsaoResources();
        }
        if (enableV5RayTracing_) {
            previewLog("VulkanGpuRenderer: createV5HistoryResources");
            createV5HistoryResources();
        }
        previewLog("VulkanGpuRenderer: createMsaaColorResources");
        createMsaaColorResources();
        previewLog("VulkanGpuRenderer: createFramebuffers");
        createFramebuffers();
        previewLog("VulkanGpuRenderer: createBuffers");
        createBuffers();
        if (enableV5RayTracing_) {
            previewLog("VulkanGpuRenderer: createV5AccelerationStructures");
            createV5AccelerationStructures();
        }
        previewLog("VulkanGpuRenderer: createTextureResources");
        createTextureResources();
        previewLog("VulkanGpuRenderer: createDescriptors");
        createDescriptors();
        if (enableV4Ssao_) {
            previewLog("VulkanGpuRenderer: createV4ComposeDescriptors");
            createV4ComposeDescriptors();
        }
        if (enableV5RayTracing_) {
            previewLog("VulkanGpuRenderer: createV5RayTracingDescriptors");
            createV5RayTracingDescriptors();
        }
        previewLog(
            "VulkanGpuRenderer: materialSets=" + std::to_string(materialTextures_.size())
            + " batches=" + std::to_string(geometry_.batches.size())
            + " lights=" + std::to_string(geometry_.lights.size())
            + " sphereInstances=" + std::to_string(geometry_.sphereInstances.size())
            + " v5RayTracing=" + (enableV5RayTracing_ ? std::string("on") : std::string("off"))
        );
        previewLog("VulkanGpuRenderer: createPipelines (factory)");
        auto pipelines = createPipelines(device_, msaaSamples_, renderPasses,
            descriptorSetLayout_, v4DescriptorSetLayout_, v5DescriptorSetLayout_,
            enableV4Ssao_, enableV5RayTracing_, gbufferSamples_);
        pipelineLayout_ = pipelines.forwardLayout;
        v4ComposePipelineLayout_ = pipelines.v4ComposeLayout;
        v5RayTracingPipelineLayout_ = pipelines.v5RayTracingLayout;
        pipeline_ = pipelines.forward;
        skyPipeline_ = pipelines.sky;
        shadowPipeline_ = pipelines.shadow;
        gbufferPipeline_ = pipelines.gbuffer;
        instancedGBufferPipeline_ = pipelines.gbufferInstanced;
        ssaoPipeline_ = pipelines.ssao;
        ssaoBlurPipeline_ = pipelines.ssaoBlur;
        v4ComposePipeline_ = pipelines.v4Compose;
        v5RayTracingPipeline_ = pipelines.v5Compute;
        v5DenoisePipeline_ = pipelines.v5Denoise;
        v5DownsamplePipeline_ = pipelines.v5Downsample;
        previewLog("VulkanGpuRenderer: createCommands");
        createCommands();
        previewLog("VulkanGpuRenderer: createSync");
        createSync();
        previewLog("VulkanGpuRenderer: ready");
    }

    ~VulkanGpuRenderer() {
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
        }
        if (device_ != VK_NULL_HANDLE && imageAvailable_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, imageAvailable_, nullptr);
        if (device_ != VK_NULL_HANDLE && renderFinished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, renderFinished_, nullptr);
        if (device_ != VK_NULL_HANDLE && inFlight_ != VK_NULL_HANDLE) vkDestroyFence(device_, inFlight_, nullptr);
        if (device_ != VK_NULL_HANDLE && commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, gbufferFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, ssaoFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, ssaoBlurFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4ComposePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, v4ComposePipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, ssaoBlurPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, ssaoPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5DownsamplePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, v5DownsamplePipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5DenoisePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, v5DenoisePipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5RayTracingPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, v5RayTracingPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && instancedGBufferPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, instancedGBufferPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, gbufferPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, shadowPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && skyPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, skyPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4ComposePipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, v4ComposePipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5RayTracingPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, v5RayTracingPipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5DescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, v5DescriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && v5DescriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, v5DescriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4DescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, v4DescriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4DescriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, v4DescriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && textureSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, textureSampler_, nullptr);
        destroyAccelerationStructure(v5Tlas_);
        destroyAccelerationStructure(v5Blas_);
        for (MaterialTextureResources& material : materialTextures_) {
            for (TextureResource& texture : material.textures) {
                if (device_ != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
                if (device_ != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
                if (device_ != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
            }
        }
        for (TextureResource& texture : sharedTextures_) {
            if (device_ != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && uniformBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, uniformBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && uniformMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, uniformMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && lightBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, lightBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && lightMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, lightMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereInstanceBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, sphereInstanceBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereInstanceMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, sphereInstanceMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, sphereVertexBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, sphereVertexMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexMemory_, nullptr);
        for (VkFramebuffer framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, msaaColorView_, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, msaaColorImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, msaaColorMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, depthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, depthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, depthMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, shadowDepthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, shadowDepthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, shadowDepthMemory_, nullptr);
        for (TextureResource& target : gbufferTargets_) {
            if (device_ != VK_NULL_HANDLE && target.view != VK_NULL_HANDLE) vkDestroyImageView(device_, target.view, nullptr);
            if (device_ != VK_NULL_HANDLE && target.image != VK_NULL_HANDLE) vkDestroyImage(device_, target.image, nullptr);
            if (device_ != VK_NULL_HANDLE && target.memory != VK_NULL_HANDLE) vkFreeMemory(device_, target.memory, nullptr);
        }
        for (TextureResource& target : gbufferMsColor_) {
            if (device_ != VK_NULL_HANDLE && target.view != VK_NULL_HANDLE) vkDestroyImageView(device_, target.view, nullptr);
            if (device_ != VK_NULL_HANDLE && target.image != VK_NULL_HANDLE) vkDestroyImage(device_, target.image, nullptr);
            if (device_ != VK_NULL_HANDLE && target.memory != VK_NULL_HANDLE) vkFreeMemory(device_, target.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, ssaoTarget_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.image != VK_NULL_HANDLE) vkDestroyImage(device_, ssaoTarget_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, ssaoTarget_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, ssaoBlurTarget_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.image != VK_NULL_HANDLE) vkDestroyImage(device_, ssaoBlurTarget_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, ssaoBlurTarget_.memory, nullptr);
        for (TextureResource& history : v5HistoryTargets_) {
            if (device_ != VK_NULL_HANDLE && history.view != VK_NULL_HANDLE) vkDestroyImageView(device_, history.view, nullptr);
            if (device_ != VK_NULL_HANDLE && history.image != VK_NULL_HANDLE) vkDestroyImage(device_, history.image, nullptr);
            if (device_ != VK_NULL_HANDLE && history.memory != VK_NULL_HANDLE) vkFreeMemory(device_, history.memory, nullptr);
        }
        for (TextureResource& history : v5ShadowHistoryTargets_) {
            if (device_ != VK_NULL_HANDLE && history.view != VK_NULL_HANDLE) vkDestroyImageView(device_, history.view, nullptr);
            if (device_ != VK_NULL_HANDLE && history.image != VK_NULL_HANDLE) vkDestroyImage(device_, history.image, nullptr);
            if (device_ != VK_NULL_HANDLE && history.memory != VK_NULL_HANDLE) vkFreeMemory(device_, history.memory, nullptr);
        }
        for (TextureResource& history : v5ReflectionHistoryTargets_) {
            if (device_ != VK_NULL_HANDLE && history.view != VK_NULL_HANDLE) vkDestroyImageView(device_, history.view, nullptr);
            if (device_ != VK_NULL_HANDLE && history.image != VK_NULL_HANDLE) vkDestroyImage(device_, history.image, nullptr);
            if (device_ != VK_NULL_HANDLE && history.memory != VK_NULL_HANDLE) vkFreeMemory(device_, history.memory, nullptr);
        }
        for (TextureResource& history : v5SurfaceHistoryTargets_) {
            if (device_ != VK_NULL_HANDLE && history.view != VK_NULL_HANDLE) vkDestroyImageView(device_, history.view, nullptr);
            if (device_ != VK_NULL_HANDLE && history.image != VK_NULL_HANDLE) vkDestroyImage(device_, history.image, nullptr);
            if (device_ != VK_NULL_HANDLE && history.memory != VK_NULL_HANDLE) vkFreeMemory(device_, history.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && v5ResolvedColor_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, v5ResolvedColor_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ResolvedColor_.image != VK_NULL_HANDLE) vkDestroyImage(device_, v5ResolvedColor_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ResolvedColor_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, v5ResolvedColor_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ShadowSignal_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, v5ShadowSignal_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ShadowSignal_.image != VK_NULL_HANDLE) vkDestroyImage(device_, v5ShadowSignal_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ShadowSignal_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, v5ShadowSignal_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ReflectionSignal_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, v5ReflectionSignal_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ReflectionSignal_.image != VK_NULL_HANDLE) vkDestroyImage(device_, v5ReflectionSignal_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && v5ReflectionSignal_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, v5ReflectionSignal_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, gbufferDepthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, gbufferDepthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, gbufferDepthMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, ssaoRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, gbufferRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
        for (VkImageView view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
        if (device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    void draw(const V1CameraSettings& camera, std::uint32_t v4DebugMode = 0) {
        const bool logFrame = frameIndex_ < 4;
        if (logFrame) previewLog("draw: wait fence");
        vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        if (logFrame) previewLog("draw: reset fence");
        vkResetFences(device_, 1, &inFlight_);

        std::uint32_t imageIndex = 0;
        if (logFrame) previewLog("draw: acquire");
        VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &imageIndex);
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
            previewLog("draw: acquire returned non-success");
            return;
        }

        if (logFrame) previewLog("draw: update uniform");
        updateUniform(camera, v4DebugMode);
        if (logFrame) previewLog("draw: record command buffer");
        recordCommandBuffer(commandBuffer_, imageIndex);

        if (logFrame) previewLog("draw: submit");
        const VkPipelineStageFlags waitStage = enableV5RayTracing_ ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable_;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer_;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished_;
        require(vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_), "vkQueueSubmit");

        if (logFrame) previewLog("draw: present");
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished_;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &imageIndex;
        vkQueuePresentKHR(presentQueue_, &present);
        if (logFrame) previewLog("draw: done");
        ++frameIndex_;
    }

    void updateGeometry(const GpuPreviewGeometry& geometry) {
        const VkDeviceSize bytes = sizeof(GpuPreviewVertex) * geometry.vertices.size();
        if (bytes > vertexBytes_) {
            throw std::runtime_error("Animated geometry grew beyond the Vulkan preview vertex buffer");
        }
        if (bytes == 0) {
            vertexCount_ = 0;
            return;
        }
        vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        void* mapped = nullptr;
        require(vkMapMemory(device_, vertexMemory_, 0, bytes, 0, &mapped), "vkMapMemory(animated vertex)");
        std::memcpy(mapped, geometry.vertices.data(), static_cast<std::size_t>(bytes));
        vkUnmapMemory(device_, vertexMemory_);
        vertexCount_ = static_cast<std::uint32_t>(geometry.vertices.size());
    }

private:
    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "LZJU VulkanRender GPU Preview";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
        appInfo.pEngineName = "LZJU VulkanRender";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;
        require(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
    }

    void createSurface() {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = GetModuleHandleW(nullptr);
        createInfo.hwnd = hwnd_;
        require(vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_), "vkCreateWin32SurfaceKHR");
    }

    QueueFamilies queueFamiliesFor(VkPhysicalDevice device) const {
        std::uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
        QueueFamilies out;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                out.graphics = i;
                out.hasGraphics = true;
            }
            VkBool32 present = VK_FALSE;
            if (surface_ != VK_NULL_HANDLE) {
                require(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present), "vkGetPhysicalDeviceSurfaceSupportKHR");
            }
            if (present) {
                out.present = i;
                out.hasPresent = true;
            }
            if (out.hasGraphics && out.hasPresent) {
                break;
            }
        }
        return out;
    }

    void selectDevice() {
        std::uint32_t count = 0;
        require(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
        if (count == 0) {
            throw std::runtime_error("No Vulkan GPU found");
        }
        std::vector<VkPhysicalDevice> devices(count);
        require(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

        for (VkPhysicalDevice device : devices) {
            const QueueFamilies families = queueFamiliesFor(device);
            if (!families.hasGraphics || !families.hasPresent) {
                continue;
            }
            std::uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
            const auto hasExt = [&](const char* name) {
                return std::any_of(extensions.begin(), extensions.end(), [name](const VkExtensionProperties& ext) {
                    return std::strcmp(ext.extensionName, name) == 0;
                });
            };
            if (!hasExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                continue;
            }
            if (enableV5RayTracing_) {
                const std::array<const char*, 6> required{
                    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                    VK_KHR_RAY_QUERY_EXTENSION_NAME,
                    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
                    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
                };
                if (!std::all_of(required.begin(), required.end(), hasExt)) {
                    continue;
                }
            }
            VkSurfaceCapabilitiesKHR caps{};
            require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
            std::uint32_t formatCount = 0;
            require(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
            if (formatCount == 0) {
                continue;
            }
            physicalDevice_ = device;
            queueFamilies_ = families;
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(device, &features);
            gpuName_ = properties.deviceName;
            msaaSamples_ = chooseSampleCount(
                properties.limits.framebufferColorSampleCounts
                & properties.limits.framebufferDepthSampleCounts
            );
            samplerAnisotropy_ = features.samplerAnisotropy == VK_TRUE;
            maxSamplerAnisotropy_ = samplerAnisotropy_ ? std::min(16.0f, properties.limits.maxSamplerAnisotropy) : 1.0f;
            if (enableV5RayTracing_) {
                VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
                bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
                VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
                accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{};
                rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
                accelerationStructure.pNext = &rayQuery;
                bufferDeviceAddress.pNext = &accelerationStructure;
                VkPhysicalDeviceFeatures2 features2{};
                features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                features2.pNext = &bufferDeviceAddress;
                vkGetPhysicalDeviceFeatures2(device, &features2);
                if (bufferDeviceAddress.bufferDeviceAddress != VK_TRUE || accelerationStructure.accelerationStructure != VK_TRUE || rayQuery.rayQuery != VK_TRUE) {
                    continue;
                }
            }
            previewLog(
                "selectDevice: " + gpuName_
                + " graphicsQueue=" + std::to_string(queueFamilies_.graphics)
                + " presentQueue=" + std::to_string(queueFamilies_.present)
                + " msaa=" + std::to_string(sampleCountValue(msaaSamples_)) + "x"
                + " anisotropy=" + (samplerAnisotropy_ ? std::to_string(maxSamplerAnisotropy_) + "x" : "off")
            );
            return;
        }
        throw std::runtime_error("No Vulkan GPU supports graphics, present, and swapchain");
    }

    void createDevice() {
        const float priority = 1.0f;
        std::array<std::uint32_t, 2> uniqueFamilies{queueFamilies_.graphics, queueFamilies_.present};
        std::vector<VkDeviceQueueCreateInfo> queues;
        queues.reserve(queueFamilies_.graphics == queueFamilies_.present ? 1u : 2u);
        for (std::uint32_t family : uniqueFamilies) {
            if (!queues.empty() && queues.front().queueFamilyIndex == family) {
                continue;
            }
            VkDeviceQueueCreateInfo queue{};
            queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            queues.push_back(queue);
        }

        std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
        VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{};
        if (enableV5RayTracing_) {
            extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
            extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

            bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
            accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructure.accelerationStructure = VK_TRUE;
            rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
            rayQuery.rayQuery = VK_TRUE;
            accelerationStructure.pNext = &rayQuery;
            bufferDeviceAddress.pNext = &accelerationStructure;
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = enableV5RayTracing_ ? &bufferDeviceAddress : nullptr;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.samplerAnisotropy = samplerAnisotropy_ ? VK_TRUE : VK_FALSE;
        createInfo.pEnabledFeatures = &enabledFeatures;
        require(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");
    }

    void fetchDeviceQueues() {
        vkGetDeviceQueue(device_, queueFamilies_.graphics, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, queueFamilies_.present, 0, &presentQueue_);
    }

    void createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);
        if (enableV5RayTracing_ && !(caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)) {
            throw std::runtime_error("Selected swapchain surface does not support VK_IMAGE_USAGE_STORAGE_BIT required by v5 realtime ray tracing");
        }

        std::uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
        swapchainFormat_ = formats.empty() ? VK_FORMAT_B8G8R8A8_UNORM : formats.front().format;
        VkColorSpaceKHR colorSpace = formats.empty() ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : formats.front().colorSpace;
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapchainFormat_ = format.format;
                colorSpace = format.colorSpace;
            }
        }

        swapchainExtent_ = caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()
            ? caps.currentExtent
            : VkExtent2D{std::clamp(width_, caps.minImageExtent.width, caps.maxImageExtent.width), std::clamp(height_, caps.minImageExtent.height, caps.maxImageExtent.height)};

        std::uint32_t imageCount = std::max(2u, caps.minImageCount);
        if (caps.maxImageCount > 0) {
            imageCount = std::min(imageCount, caps.maxImageCount);
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = swapchainFormat_;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = swapchainExtent_;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (enableV5RayTracing_) {
            createInfo.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (queueFamilies_.graphics != queueFamilies_.present) {
            std::uint32_t indices[] = {queueFamilies_.graphics, queueFamilies_.present};
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = indices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = caps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        require(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "vkCreateSwapchainKHR");

        std::uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, nullptr);
        swapchainImages_.resize(actualCount);
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, swapchainImages_.data());
        v5SwapchainImageInitialized_.assign(swapchainImages_.size(), false);
        swapchainImageViews_.reserve(swapchainImages_.size());
        for (VkImage image : swapchainImages_) {
            swapchainImageViews_.push_back(createImageView(image, swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT));
        }
    }

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        std::uint32_t mipLevels = 1,
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
        std::uint32_t layers = 1
    ) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = viewType;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = mipLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = layers;
        VkImageView view = VK_NULL_HANDLE;
        require(vkCreateImageView(device_, &createInfo, nullptr, &view), "vkCreateImageView");
        return view;
    }

    void createDepthResources() {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_D32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image.samples = msaaSamples_;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &depthImage_), "vkCreateImage(depth)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, depthImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &depthMemory_), "vkAllocateMemory(depth)");
        require(vkBindImageMemory(device_, depthImage_, depthMemory_, 0), "vkBindImageMemory(depth)");
        depthView_ = createImageView(depthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void createShadowResources() {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {kShadowAtlasWidth, kShadowAtlasHeight, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_D32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &shadowDepthImage_), "vkCreateImage(shadow depth)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, shadowDepthImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &shadowDepthMemory_), "vkAllocateMemory(shadow depth)");
        require(vkBindImageMemory(device_, shadowDepthImage_, shadowDepthMemory_, 0), "vkBindImageMemory(shadow depth)");
        shadowDepthView_ = createImageView(shadowDepthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = shadowRenderPass_;
        framebuffer.attachmentCount = 1;
        framebuffer.pAttachments = &shadowDepthView_;
        framebuffer.width = kShadowAtlasWidth;
        framebuffer.height = kShadowAtlasHeight;
        framebuffer.layers = 1;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &shadowFramebuffer_), "vkCreateFramebuffer(shadow)");

    }

    VkExtent2D v5RenderExtent() const {
        if (!enableV5RayTracing_) {
            return swapchainExtent_;
        }
        static_cast<void>(kV5QualityInternalScale);
        return {
            std::max(1u, swapchainExtent_.width * kV5InternalScale),
            std::max(1u, swapchainExtent_.height * kV5InternalScale),
        };
    }

    void createColorAttachment(TextureResource& target, VkFormat format, const char* label, VkExtent2D extent = {}) {
        if (extent.width == 0 || extent.height == 0) {
            extent = swapchainExtent_;
        }
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {extent.width, extent.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = format;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &target.image), label);

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, target.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &target.memory), "vkAllocateMemory(gbuffer)");
        require(vkBindImageMemory(device_, target.image, target.memory, 0), "vkBindImageMemory(gbuffer)");
        target.view = createImageView(target.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createColorAttachmentMs(TextureResource& target, VkFormat format, const char* label, VkExtent2D extent, VkSampleCountFlagBits samples) {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {extent.width, extent.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = format;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image.samples = samples;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &target.image), label);
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, target.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &target.memory), label);
        require(vkBindImageMemory(device_, target.image, target.memory, 0), label);
        target.view = createImageView(target.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createStorageSampledTexture(TextureResource& target, VkFormat format, const char* label, VkExtent2D extent = {}) {
        if (extent.width == 0 || extent.height == 0) {
            extent = swapchainExtent_;
        }
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {extent.width, extent.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = format;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &target.image), label);

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, target.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &target.memory), "vkAllocateMemory(storage sampled texture)");
        require(vkBindImageMemory(device_, target.image, target.memory, 0), "vkBindImageMemory(storage sampled texture)");
        target.view = createImageView(target.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createGBufferResources() {
        const VkExtent2D extent = v5RenderExtent();
        const bool useMsaa = enableV5RayTracing_ && gbufferSamples_ != VK_SAMPLE_COUNT_1_BIT;

        // Resolved images (single-sample) — used by compute shader descriptors
        createColorAttachment(gbufferTargets_[0], kGBufferAlbedoFormat, "vkCreateImage(gbuffer albedo)", extent);
        createColorAttachment(gbufferTargets_[1], kGBufferNormalFormat, "vkCreateImage(gbuffer normal)", extent);
        createColorAttachment(gbufferTargets_[2], kGBufferWorldFormat, "vkCreateImage(gbuffer world)", extent);

        // Multisampled images for render pass attachments (when MSAA enabled)
        if (useMsaa) {
            createColorAttachmentMs(gbufferMsColor_[0], kGBufferAlbedoFormat, "vkCreateImage(gbuffer ms albedo)", extent, gbufferSamples_);
            createColorAttachmentMs(gbufferMsColor_[1], kGBufferNormalFormat, "vkCreateImage(gbuffer ms normal)", extent, gbufferSamples_);
            createColorAttachmentMs(gbufferMsColor_[2], kGBufferWorldFormat, "vkCreateImage(gbuffer ms world)", extent, gbufferSamples_);
        }

        // Depth (multisampled or single-sample)
        {
            VkImageCreateInfo depth{};
            depth.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            depth.imageType = VK_IMAGE_TYPE_2D;
            depth.extent = {extent.width, extent.height, 1};
            depth.mipLevels = 1;
            depth.arrayLayers = 1;
            depth.format = VK_FORMAT_D32_SFLOAT;
            depth.tiling = VK_IMAGE_TILING_OPTIMAL;
            depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            depth.samples = useMsaa ? gbufferSamples_ : VK_SAMPLE_COUNT_1_BIT;
            depth.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            require(vkCreateImage(device_, &depth, nullptr, &gbufferDepthImage_), "vkCreateImage(gbuffer depth)");

            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(device_, gbufferDepthImage_, &requirements);
            VkMemoryAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            require(vkAllocateMemory(device_, &allocate, nullptr, &gbufferDepthMemory_), "vkAllocateMemory(gbuffer depth)");
            require(vkBindImageMemory(device_, gbufferDepthImage_, gbufferDepthMemory_, 0), "vkBindImageMemory(gbuffer depth)");
            gbufferDepthView_ = createImageView(gbufferDepthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        // Framebuffer: multisampled views first, then resolved
        std::vector<VkImageView> fbViews;
        fbViews.reserve(7);
        if (useMsaa) {
            fbViews.push_back(gbufferMsColor_[0].view);
            fbViews.push_back(gbufferMsColor_[1].view);
            fbViews.push_back(gbufferMsColor_[2].view);
        } else {
            fbViews.push_back(gbufferTargets_[0].view);
            fbViews.push_back(gbufferTargets_[1].view);
            fbViews.push_back(gbufferTargets_[2].view);
        }
        fbViews.push_back(gbufferDepthView_);
        if (useMsaa) {
            fbViews.push_back(gbufferTargets_[0].view);
            fbViews.push_back(gbufferTargets_[1].view);
            fbViews.push_back(gbufferTargets_[2].view);
        }
        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = gbufferRenderPass_;
        framebuffer.attachmentCount = static_cast<std::uint32_t>(fbViews.size());
        framebuffer.pAttachments = fbViews.data();
        framebuffer.width = extent.width;
        framebuffer.height = extent.height;
        framebuffer.layers = 1;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &gbufferFramebuffer_), "vkCreateFramebuffer(gbuffer)");
    }

    void createV5HistoryResources() {
        const VkExtent2D extent = v5RenderExtent();
        createStorageSampledTexture(v5HistoryTargets_[0], kV5HistoryFormat, "vkCreateImage(v5 history 0)", extent);
        createStorageSampledTexture(v5HistoryTargets_[1], kV5HistoryFormat, "vkCreateImage(v5 history 1)", extent);
        createStorageSampledTexture(v5ShadowSignal_, kV5ShadowSignalFormat, "vkCreateImage(v5 shadow signal)", extent);
        createStorageSampledTexture(v5ReflectionSignal_, kV5ReflectionSignalFormat, "vkCreateImage(v5 reflection signal)", extent);
        createStorageSampledTexture(v5ShadowHistoryTargets_[0], kV5ShadowSignalFormat, "vkCreateImage(v5 shadow history 0)", extent);
        createStorageSampledTexture(v5ShadowHistoryTargets_[1], kV5ShadowSignalFormat, "vkCreateImage(v5 shadow history 1)", extent);
        createStorageSampledTexture(v5ReflectionHistoryTargets_[0], kV5ReflectionSignalFormat, "vkCreateImage(v5 reflection history 0)", extent);
        createStorageSampledTexture(v5ReflectionHistoryTargets_[1], kV5ReflectionSignalFormat, "vkCreateImage(v5 reflection history 1)", extent);
        createStorageSampledTexture(v5SurfaceHistoryTargets_[0], kV5SurfaceHistoryFormat, "vkCreateImage(v5 surface history 0)", extent);
        createStorageSampledTexture(v5SurfaceHistoryTargets_[1], kV5SurfaceHistoryFormat, "vkCreateImage(v5 surface history 1)", extent);
        createStorageSampledTexture(v5ResolvedColor_, kV5HistoryFormat, "vkCreateImage(v5 resolved color)", extent);
        v5HistoryInitialized_ = {false, false};
        v5ShadowHistoryInitialized_ = {false, false};
        v5ReflectionHistoryInitialized_ = {false, false};
        v5SurfaceHistoryInitialized_ = {false, false};
        v5SignalInitialized_ = false;
        v5ResolvedColorInitialized_ = false;
    }

    void createSsaoResources() {
        createColorAttachment(ssaoTarget_, kSsaoFormat, "vkCreateImage(ssao raw)");
        createColorAttachment(ssaoBlurTarget_, kSsaoFormat, "vkCreateImage(ssao blur)");

        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = ssaoRenderPass_;
        framebuffer.attachmentCount = 1;
        framebuffer.width = swapchainExtent_.width;
        framebuffer.height = swapchainExtent_.height;
        framebuffer.layers = 1;

        framebuffer.pAttachments = &ssaoTarget_.view;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &ssaoFramebuffer_), "vkCreateFramebuffer(ssao)");

        framebuffer.pAttachments = &ssaoBlurTarget_.view;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &ssaoBlurFramebuffer_), "vkCreateFramebuffer(ssao blur)");
    }

    void createMsaaColorResources() {
        if (msaaSamples_ == VK_SAMPLE_COUNT_1_BIT) {
            return;
        }

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = swapchainFormat_;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image.samples = msaaSamples_;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &msaaColorImage_), "vkCreateImage(msaa color)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, msaaColorImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &msaaColorMemory_), "vkAllocateMemory(msaa color)");
        require(vkBindImageMemory(device_, msaaColorImage_, msaaColorMemory_, 0), "vkBindImageMemory(msaa color)");
        msaaColorView_ = createImageView(msaaColorImage_, swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createFramebuffers() {
        framebuffers_.reserve(swapchainImageViews_.size());
        for (VkImageView view : swapchainImageViews_) {
            std::array<VkImageView, 3> attachmentsWithMsaa{msaaColorView_, depthView_, view};
            std::array<VkImageView, 2> attachmentsWithoutMsaa{view, depthView_};
            const bool useMsaa = msaaSamples_ != VK_SAMPLE_COUNT_1_BIT;
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass_;
            createInfo.attachmentCount = useMsaa
                ? static_cast<std::uint32_t>(attachmentsWithMsaa.size())
                : static_cast<std::uint32_t>(attachmentsWithoutMsaa.size());
            createInfo.pAttachments = useMsaa ? attachmentsWithMsaa.data() : attachmentsWithoutMsaa.data();
            createInfo.width = swapchainExtent_.width;
            createInfo.height = swapchainExtent_.height;
            createInfo.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            require(vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            framebuffers_.push_back(framebuffer);
        }
    }

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer& buffer,
        VkDeviceMemory& memory,
        VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VkMemoryAllocateFlags allocationFlags = 0
    ) {
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateBuffer(device_, &createInfo, nullptr, &buffer), "vkCreateBuffer");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, buffer, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        VkMemoryAllocateFlagsInfo allocationFlagsInfo{};
        if (allocationFlags != 0) {
            allocationFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocationFlagsInfo.flags = allocationFlags;
            allocate.pNext = &allocationFlagsInfo;
        }
        allocate.memoryTypeIndex = findMemoryType(
            physicalDevice_,
            requirements.memoryTypeBits,
            memoryProperties
        );
        require(vkAllocateMemory(device_, &allocate, nullptr, &memory), "vkAllocateMemory(buffer)");
        require(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
    }

    void createBuffers() {
        vertexCount_ = static_cast<std::uint32_t>(geometry_.vertices.size());
        vertexBytes_ = std::max<VkDeviceSize>(1, sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        VkBufferUsageFlags vertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VkMemoryAllocateFlags vertexAllocationFlags = 0;
        if (enableV5RayTracing_) {
            vertexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            vertexAllocationFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        }
        createBuffer(vertexBytes_, vertexUsage, vertexBuffer_, vertexMemory_, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexAllocationFlags);
        void* mapped = nullptr;
        require(vkMapMemory(device_, vertexMemory_, 0, vertexBytes_, 0, &mapped), "vkMapMemory(vertex)");
        std::memcpy(mapped, geometry_.vertices.data(), sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        vkUnmapMemory(device_, vertexMemory_);

        createBuffer(sizeof(CameraUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffer_, uniformMemory_);

        lightCount_ = static_cast<std::uint32_t>(geometry_.lights.size());
        lightBytes_ = std::max<VkDeviceSize>(sizeof(GpuPreviewLight), sizeof(GpuPreviewLight) * geometry_.lights.size());
        createBuffer(lightBytes_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, lightBuffer_, lightMemory_);
        if (!geometry_.lights.empty()) {
            void* lightMapped = nullptr;
            require(vkMapMemory(device_, lightMemory_, 0, lightBytes_, 0, &lightMapped), "vkMapMemory(light buffer)");
            std::memcpy(lightMapped, geometry_.lights.data(), sizeof(GpuPreviewLight) * geometry_.lights.size());
            vkUnmapMemory(device_, lightMemory_);
        }

        sphereInstanceCount_ = static_cast<std::uint32_t>(geometry_.sphereInstances.size());
        if (!geometry_.sphereInstances.empty()) {
            const std::vector<GpuPreviewVertex> unitSphere = makeUnitSphereVertices(16, 24);
            sphereVertexCount_ = static_cast<std::uint32_t>(unitSphere.size());
            sphereVertexBytes_ = sizeof(GpuPreviewVertex) * unitSphere.size();
            createBuffer(sphereVertexBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sphereVertexBuffer_, sphereVertexMemory_);
            void* sphereMapped = nullptr;
            require(vkMapMemory(device_, sphereVertexMemory_, 0, sphereVertexBytes_, 0, &sphereMapped), "vkMapMemory(sphere vertex)");
            std::memcpy(sphereMapped, unitSphere.data(), static_cast<std::size_t>(sphereVertexBytes_));
            vkUnmapMemory(device_, sphereVertexMemory_);

            sphereInstanceBytes_ = sizeof(GpuPreviewSphereInstance) * geometry_.sphereInstances.size();
            createBuffer(sphereInstanceBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sphereInstanceBuffer_, sphereInstanceMemory_);
            void* instanceMapped = nullptr;
            require(vkMapMemory(device_, sphereInstanceMemory_, 0, sphereInstanceBytes_, 0, &instanceMapped), "vkMapMemory(sphere instance)");
            std::memcpy(instanceMapped, geometry_.sphereInstances.data(), static_cast<std::size_t>(sphereInstanceBytes_));
            vkUnmapMemory(device_, sphereInstanceMemory_);
        }
    }

    VkCommandBuffer beginOneTimeCommands() {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = queueFamilies_.graphics;
        require(vkCreateCommandPool(device_, &pool, nullptr, &uploadCommandPool_), "vkCreateCommandPool(upload)");

        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = uploadCommandPool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        require(vkAllocateCommandBuffers(device_, &allocate, &commandBuffer), "vkAllocateCommandBuffers(upload)");

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer(upload)");
        return commandBuffer;
    }

    void endOneTimeCommands(VkCommandBuffer commandBuffer) {
        require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(upload)");
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        require(vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
        require(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(upload)");
        vkFreeCommandBuffers(device_, uploadCommandPool_, 1, &commandBuffer);
        vkDestroyCommandPool(device_, uploadCommandPool_, nullptr);
        uploadCommandPool_ = VK_NULL_HANDLE;
    }

    VkDeviceAddress bufferDeviceAddress(VkBuffer buffer) const {
        VkBufferDeviceAddressInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buffer;
        return vkGetBufferDeviceAddressKHR(device_, &info);
    }

    void createAccelerationStorage(
        VkAccelerationStructureTypeKHR type,
        VkDeviceSize size,
        AccelerationStructureResource& resource
    ) {
        createBuffer(
            size,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            resource.buffer,
            resource.memory,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        );

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = resource.buffer;
        createInfo.size = size;
        createInfo.type = type;
        require(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &resource.handle), "vkCreateAccelerationStructureKHR");

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = resource.handle;
        resource.address = vkGetAccelerationStructureDeviceAddressKHR(device_, &addressInfo);
    }

    void destroyAccelerationStructure(AccelerationStructureResource& resource) {
        if (device_ != VK_NULL_HANDLE && resource.handle != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(device_, resource.handle, nullptr);
            resource.handle = VK_NULL_HANDLE;
        }
        if (device_ != VK_NULL_HANDLE && resource.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, resource.buffer, nullptr);
            resource.buffer = VK_NULL_HANDLE;
        }
        if (device_ != VK_NULL_HANDLE && resource.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, resource.memory, nullptr);
            resource.memory = VK_NULL_HANDLE;
        }
        resource.address = 0;
    }

    void createV5AccelerationStructures() {
        const std::uint32_t primitiveCount = vertexCount_ / 3u;
        if (primitiveCount == 0) {
            throw std::runtime_error("V5 realtime ray tracing requires triangle geometry for BLAS build");
        }

        VkDeviceOrHostAddressConstKHR vertexAddress{};
        vertexAddress.deviceAddress = bufferDeviceAddress(vertexBuffer_);

        VkAccelerationStructureGeometryKHR blasGeometry{};
        blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        blasGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        blasGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        blasGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        blasGeometry.geometry.triangles.vertexData = vertexAddress;
        blasGeometry.geometry.triangles.vertexStride = sizeof(GpuPreviewVertex);
        blasGeometry.geometry.triangles.maxVertex = vertexCount_ - 1u;
        blasGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo{};
        blasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        blasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        blasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blasBuildInfo.geometryCount = 1;
        blasBuildInfo.pGeometries = &blasGeometry;

        VkAccelerationStructureBuildSizesInfoKHR blasSizes{};
        blasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device_,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &blasBuildInfo,
            &primitiveCount,
            &blasSizes
        );
        createAccelerationStorage(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, blasSizes.accelerationStructureSize, v5Blas_);

        VkBuffer blasScratch = VK_NULL_HANDLE;
        VkDeviceMemory blasScratchMemory = VK_NULL_HANDLE;
        createBuffer(
            blasSizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            blasScratch,
            blasScratchMemory,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        );
        VkDeviceOrHostAddressKHR blasScratchAddress{};
        blasScratchAddress.deviceAddress = bufferDeviceAddress(blasScratch);
        blasBuildInfo.dstAccelerationStructure = v5Blas_.handle;
        blasBuildInfo.scratchData = blasScratchAddress;

        VkAccelerationStructureBuildRangeInfoKHR blasRange{};
        blasRange.primitiveCount = primitiveCount;
        const VkAccelerationStructureBuildRangeInfoKHR* blasRangePtr = &blasRange;
        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &blasBuildInfo, &blasRangePtr);
        endOneTimeCommands(commandBuffer);
        vkDestroyBuffer(device_, blasScratch, nullptr);
        vkFreeMemory(device_, blasScratchMemory, nullptr);

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform.matrix[0][0] = 1.0f;
        instance.transform.matrix[1][1] = 1.0f;
        instance.transform.matrix[2][2] = 1.0f;
        instance.instanceCustomIndex = 0;
        instance.mask = 0xff;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = v5Blas_.address;

        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
        createBuffer(
            sizeof(instance),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            instanceBuffer,
            instanceMemory,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        );
        void* mapped = nullptr;
        require(vkMapMemory(device_, instanceMemory, 0, sizeof(instance), 0, &mapped), "vkMapMemory(tlas instance)");
        std::memcpy(mapped, &instance, sizeof(instance));
        vkUnmapMemory(device_, instanceMemory);

        VkDeviceOrHostAddressConstKHR instanceAddress{};
        instanceAddress.deviceAddress = bufferDeviceAddress(instanceBuffer);
        VkAccelerationStructureGeometryKHR tlasGeometry{};
        tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        tlasGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        tlasGeometry.geometry.instances.data = instanceAddress;

        const std::uint32_t instanceCount = 1;
        VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
        tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlasBuildInfo.geometryCount = 1;
        tlasBuildInfo.pGeometries = &tlasGeometry;

        VkAccelerationStructureBuildSizesInfoKHR tlasSizes{};
        tlasSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device_,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &tlasBuildInfo,
            &instanceCount,
            &tlasSizes
        );
        createAccelerationStorage(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, tlasSizes.accelerationStructureSize, v5Tlas_);

        VkBuffer tlasScratch = VK_NULL_HANDLE;
        VkDeviceMemory tlasScratchMemory = VK_NULL_HANDLE;
        createBuffer(
            tlasSizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            tlasScratch,
            tlasScratchMemory,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        );
        VkDeviceOrHostAddressKHR tlasScratchAddress{};
        tlasScratchAddress.deviceAddress = bufferDeviceAddress(tlasScratch);
        tlasBuildInfo.dstAccelerationStructure = v5Tlas_.handle;
        tlasBuildInfo.scratchData = tlasScratchAddress;

        VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
        tlasRange.primitiveCount = instanceCount;
        const VkAccelerationStructureBuildRangeInfoKHR* tlasRangePtr = &tlasRange;
        commandBuffer = beginOneTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildInfo, &tlasRangePtr);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, tlasScratch, nullptr);
        vkFreeMemory(device_, tlasScratchMemory, nullptr);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);

        previewLog("createV5AccelerationStructures: triangles=" + std::to_string(primitiveCount) + " tlas=ready");
    }

    void transitionImage(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t mipLevels = 1
    ) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels) {
        std::int32_t mipWidth = static_cast<std::int32_t>(width);
        std::int32_t mipHeight = static_cast<std::int32_t>(height);

        for (std::uint32_t i = 1; i < mipLevels; ++i) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            vkCmdBlitImage(
                commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
            mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );
    }

    void createSampledTexture(
        const std::filesystem::path& path,
        const std::array<stbi_uc, 4>& fallback,
        TextureResource& texture
    ) {
        int width = 1;
        int height = 1;
        int channels = 4;
        stbi_uc* loaded = nullptr;
        const stbi_uc* pixels = fallback.data();
        if (!path.empty()) {
            loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (loaded) {
                pixels = loaded;
                channels = 4;
            } else {
                previewLog("createTextureResources: failed to load " + path.string());
            }
        }

        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
        const bool canLinearBlit = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
        texture.mipLevels = canLinearBlit
            ? mipLevelsFor(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))
            : 1u;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(texture staging)");
        std::memcpy(mapped, pixels, static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);
        if (loaded) {
            stbi_image_free(loaded);
        }

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        image.mipLevels = texture.mipLevels;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(texture)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(texture)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(texture)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        if (texture.mipLevels > 1) {
            generateMipmaps(
                commandBuffer,
                texture.image,
                static_cast<std::uint32_t>(width),
                static_cast<std::uint32_t>(height),
                texture.mipLevels
            );
        } else {
            transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, texture.mipLevels);
    }

    void createRgbeCubeTexture(const std::filesystem::path& path, const RgbaFloat& fallback, TextureResource& texture) {
        int width = 1;
        int height = 6;
        int channels = 4;
        stbi_uc* loaded = nullptr;
        std::vector<RgbaFloat> pixels;

        if (!path.empty()) {
            loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (!loaded) {
                previewLog("createTextureResources: failed to load cube " + path.string());
            }
        }

        if (loaded && width > 0 && height == width * 6) {
            pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                pixels[i] = unpackRgbe(loaded + i * 4u);
            }
        } else {
            if (loaded) {
                previewLog("createTextureResources: cube texture must be a vertical 6-face strip: " + path.string());
                stbi_image_free(loaded);
                loaded = nullptr;
            }
            width = 1;
            height = 6;
            pixels.assign(6, fallback);
        }
        if (loaded) {
            stbi_image_free(loaded);
        }

        const std::uint32_t faceSize = static_cast<std::uint32_t>(width);
        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(pixels.size()) * sizeof(RgbaFloat);
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(cube staging)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {faceSize, faceSize, 1};
        image.mipLevels = 1;
        image.arrayLayers = 6;
        image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(cube texture)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(cube texture)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(cube texture)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 6;
        copy.imageExtent = {faceSize, faceSize, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.mipLevels = 1;
        texture.view = createImageView(
            texture.image,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            VK_IMAGE_VIEW_TYPE_CUBE,
            6
        );
    }

    void createEnvironmentBrdfTexture(TextureResource& texture) {
        constexpr std::uint32_t size = 128;
        std::vector<RgFloat> pixels(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        for (std::uint32_t y = 0; y < size; ++y) {
            for (std::uint32_t x = 0; x < size; ++x) {
                const float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                pixels[static_cast<std::size_t>(y) * size + x] = integrateEnvironmentBrdf(ndotv, roughness);
            }
        }

        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(pixels.size()) * sizeof(RgFloat);
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(brdf staging)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {size, size, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_R32G32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(environment brdf)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(environment brdf)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(environment brdf)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {size, size, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.mipLevels = 1;
        texture.view = createImageView(texture.image, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureResources() {
        const std::size_t materialCount = std::max<std::size_t>(1, geometry_.materials.size());
        materialTextures_.resize(materialCount);
        for (std::size_t i = 0; i < materialTextures_.size(); ++i) {
            const GpuPreviewGeometry::MaterialTextures material = i < geometry_.materials.size()
                ? geometry_.materials[i]
                : GpuPreviewGeometry::MaterialTextures{};
            createSampledTexture(material.albedoTexturePath, {255, 255, 255, 255}, materialTextures_[i].textures[0]);
            createSampledTexture(material.normalTexturePath, {128, 128, 255, 255}, materialTextures_[i].textures[1]);
            createSampledTexture(material.roughnessTexturePath, {178, 178, 178, 255}, materialTextures_[i].textures[2]);
            createSampledTexture(material.displacementTexturePath, {128, 128, 128, 255}, materialTextures_[i].textures[3]);
        }

        createRgbeCubeTexture(geometry_.environmentBackgroundTexturePath, {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentBackgroundTexture]);
        createRgbeCubeTexture(geometry_.environmentDiffuseTexturePath, {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentDiffuseTexture]);
        for (std::uint32_t i = 0; i < kEnvironmentSpecularTextureCount; ++i) {
            createRgbeCubeTexture(geometry_.environmentSpecularTexturePaths[i], {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentSpecularTextureBase + i]);
        }
        createEnvironmentBrdfTexture(sharedTextures_[kEnvironmentBrdfTextureIndex]);
        maxTextureMipLevels_ = 1;
        for (const MaterialTextureResources& material : materialTextures_) {
            for (const TextureResource& texture : material.textures) {
                maxTextureMipLevels_ = std::max(maxTextureMipLevels_, texture.mipLevels);
            }
        }
        for (const TextureResource& texture : sharedTextures_) {
            maxTextureMipLevels_ = std::max(maxTextureMipLevels_, texture.mipLevels);
        }

        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.minLod = 0.0f;
        sampler.maxLod = static_cast<float>(maxTextureMipLevels_ - 1);
        sampler.anisotropyEnable = samplerAnisotropy_ ? VK_TRUE : VK_FALSE;
        sampler.maxAnisotropy = maxSamplerAnisotropy_;
        require(vkCreateSampler(device_, &sampler, nullptr, &textureSampler_), "vkCreateSampler(texture)");
        previewLog(
            "createTextureResources: mipLevels="
            + std::to_string(maxTextureMipLevels_)
            + " sampler=linear-mipmap-linear anisotropy="
            + (samplerAnisotropy_ ? std::to_string(maxSamplerAnisotropy_) + "x" : "off")
            + " edgeAA=g-buffer"
            + (enableV5RayTracing_ ? " taa=halton16-surface-validated-resolve" : "")
            + (enableV5RayTracing_ ? " denoise=hybrid-split-signal-temporal-bilateral" : "")
            + (enableV5RayTracing_ ? " mode=realtime-hybrid-rt" : "")
            + (enableV5RayTracing_ ? (" internalScale=" + std::to_string(kV5InternalScale) + "x") : "")
        );
    }

    void createDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, kMaterialTextureCount + kSharedTextureCount + 3> bindings{};
        bindings[0] = cameraBinding;
        for (std::uint32_t i = 0; i < kMaterialTextureCount; ++i) {
            bindings[1 + i].binding = 1 + i;
            bindings[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[1 + i].descriptorCount = 1;
            bindings[1 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 5;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[5] = samplerBinding;
        for (std::uint32_t i = 0; i < kSharedTextureCount; ++i) {
            bindings[6 + i].binding = 6 + i;
            bindings[6 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[6 + i].descriptorCount = 1;
            bindings[6 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].binding = kDirectionalShadowTextureBinding;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].descriptorCount = 1;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &descriptorSetLayout_), "vkCreateDescriptorSetLayout");

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<std::uint32_t>(materialTextures_.size());
        poolSizes[1].descriptorCount = static_cast<std::uint32_t>((kMaterialTextureCount + kSharedTextureCount + 1u) * materialTextures_.size());
        poolSizes[2].descriptorCount = static_cast<std::uint32_t>(materialTextures_.size());
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = static_cast<std::uint32_t>(materialTextures_.size());
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptorPool_), "vkCreateDescriptorPool");

        std::vector<VkDescriptorSetLayout> layouts(materialTextures_.size(), descriptorSetLayout_);
        descriptorSets_.resize(materialTextures_.size());
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = descriptorPool_;
        allocate.descriptorSetCount = static_cast<std::uint32_t>(descriptorSets_.size());
        allocate.pSetLayouts = layouts.data();
        require(vkAllocateDescriptorSets(device_, &allocate, descriptorSets_.data()), "vkAllocateDescriptorSets");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;

        std::vector<std::array<VkDescriptorImageInfo, kMaterialTextureCount + kSharedTextureCount + 1>> imageInfos(descriptorSets_.size());
        std::vector<std::array<VkWriteDescriptorSet, kMaterialTextureCount + kSharedTextureCount + 3>> writes(descriptorSets_.size());
        for (std::size_t setIndex = 0; setIndex < descriptorSets_.size(); ++setIndex) {
            std::array<VkWriteDescriptorSet, kMaterialTextureCount + kSharedTextureCount + 3>& setWrites = writes[setIndex];
            setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[0].dstSet = descriptorSets_[setIndex];
            setWrites[0].dstBinding = 0;
            setWrites[0].descriptorCount = 1;
            setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            setWrites[0].pBufferInfo = &bufferInfo;

            for (std::uint32_t i = 0; i < kMaterialTextureCount; ++i) {
                imageInfos[setIndex][i].imageView = materialTextures_[setIndex].textures[i].view;
                imageInfos[setIndex][i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                setWrites[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                setWrites[1 + i].dstSet = descriptorSets_[setIndex];
                setWrites[1 + i].dstBinding = 1 + i;
                setWrites[1 + i].descriptorCount = 1;
                setWrites[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                setWrites[1 + i].pImageInfo = &imageInfos[setIndex][i];
            }

            setWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[5].dstSet = descriptorSets_[setIndex];
            setWrites[5].dstBinding = 5;
            setWrites[5].descriptorCount = 1;
            setWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            setWrites[5].pImageInfo = &samplerInfo;

            for (std::uint32_t i = 0; i < kSharedTextureCount; ++i) {
                const std::uint32_t infoIndex = kMaterialTextureCount + i;
                imageInfos[setIndex][infoIndex].imageView = sharedTextures_[i].view;
                imageInfos[setIndex][infoIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                setWrites[6 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                setWrites[6 + i].dstSet = descriptorSets_[setIndex];
                setWrites[6 + i].dstBinding = 6 + i;
                setWrites[6 + i].descriptorCount = 1;
                setWrites[6 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                setWrites[6 + i].pImageInfo = &imageInfos[setIndex][infoIndex];
            }
            const std::uint32_t shadowInfoIndex = kMaterialTextureCount + kSharedTextureCount;
            imageInfos[setIndex][shadowInfoIndex].imageView = shadowDepthView_;
            imageInfos[setIndex][shadowInfoIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].dstSet = descriptorSets_[setIndex];
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].dstBinding = kDirectionalShadowTextureBinding;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].descriptorCount = 1;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].pImageInfo = &imageInfos[setIndex][shadowInfoIndex];
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
        }
    }

    void createV4ComposeDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 5;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding albedoBinding{};
        albedoBinding.binding = kGBufferAlbedoBinding;
        albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        albedoBinding.descriptorCount = 1;
        albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding normalBinding = albedoBinding;
        normalBinding.binding = kGBufferNormalBinding;
        VkDescriptorSetLayoutBinding worldBinding = albedoBinding;
        worldBinding.binding = kGBufferWorldBinding;
        VkDescriptorSetLayoutBinding ssaoBinding = albedoBinding;
        ssaoBinding.binding = kSsaoRawBinding;
        VkDescriptorSetLayoutBinding ssaoBlurBinding = albedoBinding;
        ssaoBlurBinding.binding = kSsaoBlurBinding;
        VkDescriptorSetLayoutBinding manyLightBinding{};
        manyLightBinding.binding = kV4ManyLightBufferBinding;
        manyLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        manyLightBinding.descriptorCount = 1;
        manyLightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 8> bindings{
            cameraBinding,
            samplerBinding,
            albedoBinding,
            normalBinding,
            worldBinding,
            ssaoBinding,
            ssaoBlurBinding,
            manyLightBinding,
        };
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &v4DescriptorSetLayout_), "vkCreateDescriptorSetLayout(v4)");

        std::array<VkDescriptorPoolSize, 4> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = 1;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[2].descriptorCount = 5;
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[3].descriptorCount = 1;
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        pool.maxSets = 1;
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &v4DescriptorPool_), "vkCreateDescriptorPool(v4)");

        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = v4DescriptorPool_;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &v4DescriptorSetLayout_;
        require(vkAllocateDescriptorSets(device_, &allocate, &v4DescriptorSet_), "vkAllocateDescriptorSets(v4)");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;
        VkDescriptorImageInfo albedoInfo{};
        albedoInfo.imageView = gbufferTargets_[0].view;
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageView = gbufferTargets_[1].view;
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo worldInfo{};
        worldInfo.imageView = gbufferTargets_[2].view;
        worldInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo ssaoInfo{};
        ssaoInfo.imageView = ssaoTarget_.view;
        ssaoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo ssaoBlurInfo{};
        ssaoBlurInfo.imageView = ssaoBlurTarget_.view;
        ssaoBlurInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorBufferInfo manyLightInfo{};
        manyLightInfo.buffer = lightBuffer_;
        manyLightInfo.offset = 0;
        manyLightInfo.range = lightBytes_;

        std::array<VkWriteDescriptorSet, 8> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = v4DescriptorSet_;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &bufferInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = v4DescriptorSet_;
        writes[1].dstBinding = 5;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[1].pImageInfo = &samplerInfo;
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = v4DescriptorSet_;
        writes[2].dstBinding = kGBufferAlbedoBinding;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[2].pImageInfo = &albedoInfo;
        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = v4DescriptorSet_;
        writes[3].dstBinding = kGBufferNormalBinding;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[3].pImageInfo = &normalInfo;
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = v4DescriptorSet_;
        writes[4].dstBinding = kGBufferWorldBinding;
        writes[4].descriptorCount = 1;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[4].pImageInfo = &worldInfo;
        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = v4DescriptorSet_;
        writes[5].dstBinding = kSsaoRawBinding;
        writes[5].descriptorCount = 1;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[5].pImageInfo = &ssaoInfo;
        writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[6].dstSet = v4DescriptorSet_;
        writes[6].dstBinding = kSsaoBlurBinding;
        writes[6].descriptorCount = 1;
        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[6].pImageInfo = &ssaoBlurInfo;
        writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[7].dstSet = v4DescriptorSet_;
        writes[7].dstBinding = kV4ManyLightBufferBinding;
        writes[7].descriptorCount = 1;
        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[7].pBufferInfo = &manyLightInfo;
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void createV5RayTracingDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding outputBinding{};
        outputBinding.binding = 1;
        outputBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputBinding.descriptorCount = 1;
        outputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 5;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding albedoBinding{};
        albedoBinding.binding = kGBufferAlbedoBinding;
        albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        albedoBinding.descriptorCount = 1;
        albedoBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding normalBinding = albedoBinding;
        normalBinding.binding = kGBufferNormalBinding;
        VkDescriptorSetLayoutBinding worldBinding = albedoBinding;
        worldBinding.binding = kGBufferWorldBinding;
        VkDescriptorSetLayoutBinding tlasBinding{};
        tlasBinding.binding = kV5SceneTlasBinding;
        tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        tlasBinding.descriptorCount = 1;
        tlasBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutBinding historyInputBinding = albedoBinding;
        historyInputBinding.binding = kV5HistoryInputBinding;
        VkDescriptorSetLayoutBinding historyOutputBinding{};
        historyOutputBinding.binding = kV5HistoryOutputBinding;
        historyOutputBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        historyOutputBinding.descriptorCount = 1;
        historyOutputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutBinding shadowSignalBinding = historyOutputBinding;
        shadowSignalBinding.binding = kV5ShadowSignalBinding;
        VkDescriptorSetLayoutBinding reflectionSignalBinding = historyOutputBinding;
        reflectionSignalBinding.binding = kV5ReflectionSignalBinding;
        VkDescriptorSetLayoutBinding shadowHistoryInputBinding = albedoBinding;
        shadowHistoryInputBinding.binding = kV5ShadowHistoryInputBinding;
        VkDescriptorSetLayoutBinding shadowHistoryOutputBinding = historyOutputBinding;
        shadowHistoryOutputBinding.binding = kV5ShadowHistoryOutputBinding;
        VkDescriptorSetLayoutBinding reflectionHistoryInputBinding = albedoBinding;
        reflectionHistoryInputBinding.binding = kV5ReflectionHistoryInputBinding;
        VkDescriptorSetLayoutBinding reflectionHistoryOutputBinding = historyOutputBinding;
        reflectionHistoryOutputBinding.binding = kV5ReflectionHistoryOutputBinding;
        VkDescriptorSetLayoutBinding surfaceHistoryInputBinding = albedoBinding;
        surfaceHistoryInputBinding.binding = kV5SurfaceHistoryInputBinding;
        VkDescriptorSetLayoutBinding surfaceHistoryOutputBinding = historyOutputBinding;
        surfaceHistoryOutputBinding.binding = kV5SurfaceHistoryOutputBinding;
        VkDescriptorSetLayoutBinding resolvedColorBinding = historyOutputBinding;
        resolvedColorBinding.binding = kV5ResolvedColorBinding;
        VkDescriptorSetLayoutBinding lightBinding{};
        lightBinding.binding = kV4ManyLightBufferBinding;
        lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightBinding.descriptorCount = 1;
        lightBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 19> bindings{
            cameraBinding,
            outputBinding,
            samplerBinding,
            albedoBinding,
            normalBinding,
            worldBinding,
            tlasBinding,
            historyInputBinding,
            historyOutputBinding,
            lightBinding,
            shadowSignalBinding,
            reflectionSignalBinding,
            shadowHistoryInputBinding,
            shadowHistoryOutputBinding,
            reflectionHistoryInputBinding,
            reflectionHistoryOutputBinding,
            surfaceHistoryInputBinding,
            surfaceHistoryOutputBinding,
            resolvedColorBinding,
        };
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &v5DescriptorSetLayout_), "vkCreateDescriptorSetLayout(v5 rt)");

        std::array<VkDescriptorPoolSize, 6> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 2u);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 16u);
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[2].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 2u);
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[3].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 14u);
        poolSizes[4].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        poolSizes[4].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 2u);
        poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[5].descriptorCount = static_cast<std::uint32_t>(swapchainImageViews_.size() * 2u);
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        pool.maxSets = static_cast<std::uint32_t>(swapchainImageViews_.size() * 2u);
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &v5DescriptorPool_), "vkCreateDescriptorPool(v5 rt)");

        std::vector<VkDescriptorSetLayout> layouts(swapchainImageViews_.size() * 2u, v5DescriptorSetLayout_);
        v5DescriptorSets_.resize(swapchainImageViews_.size() * 2u);
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = v5DescriptorPool_;
        allocate.descriptorSetCount = static_cast<std::uint32_t>(v5DescriptorSets_.size());
        allocate.pSetLayouts = layouts.data();
        require(vkAllocateDescriptorSets(device_, &allocate, v5DescriptorSets_.data()), "vkAllocateDescriptorSets(v5 rt)");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);

        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;

        VkDescriptorImageInfo albedoInfo{};
        albedoInfo.imageView = gbufferTargets_[0].view;
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageView = gbufferTargets_[1].view;
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo worldInfo{};
        worldInfo.imageView = gbufferTargets_[2].view;
        worldInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorBufferInfo lightInfo{};
        lightInfo.buffer = lightBuffer_;
        lightInfo.offset = 0;
        lightInfo.range = lightBytes_;

        std::vector<VkDescriptorImageInfo> outputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> historyInputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> historyOutputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> shadowSignalInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> reflectionSignalInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> shadowHistoryInputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> shadowHistoryOutputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> reflectionHistoryInputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> reflectionHistoryOutputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> surfaceHistoryInputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> surfaceHistoryOutputInfos(v5DescriptorSets_.size());
        std::vector<VkDescriptorImageInfo> resolvedColorInfos(v5DescriptorSets_.size());
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asInfos(v5DescriptorSets_.size());
        std::vector<std::array<VkWriteDescriptorSet, 19>> writes(v5DescriptorSets_.size());
        for (std::size_t i = 0; i < v5DescriptorSets_.size(); ++i) {
            const std::size_t swapchainIndex = i / 2u;
            const std::size_t pingPongIndex = i % 2u;
            outputInfos[i].imageView = swapchainImageViews_[swapchainIndex];
            outputInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            historyInputInfos[i].imageView = v5HistoryTargets_[1u - pingPongIndex].view;
            historyInputInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            historyOutputInfos[i].imageView = v5HistoryTargets_[pingPongIndex].view;
            historyOutputInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            shadowSignalInfos[i].imageView = v5ShadowSignal_.view;
            shadowSignalInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            reflectionSignalInfos[i].imageView = v5ReflectionSignal_.view;
            reflectionSignalInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            shadowHistoryInputInfos[i].imageView = v5ShadowHistoryTargets_[1u - pingPongIndex].view;
            shadowHistoryInputInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            shadowHistoryOutputInfos[i].imageView = v5ShadowHistoryTargets_[pingPongIndex].view;
            shadowHistoryOutputInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            reflectionHistoryInputInfos[i].imageView = v5ReflectionHistoryTargets_[1u - pingPongIndex].view;
            reflectionHistoryInputInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            reflectionHistoryOutputInfos[i].imageView = v5ReflectionHistoryTargets_[pingPongIndex].view;
            reflectionHistoryOutputInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            surfaceHistoryInputInfos[i].imageView = v5SurfaceHistoryTargets_[1u - pingPongIndex].view;
            surfaceHistoryInputInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            surfaceHistoryOutputInfos[i].imageView = v5SurfaceHistoryTargets_[pingPongIndex].view;
            surfaceHistoryOutputInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            resolvedColorInfos[i].imageView = v5ResolvedColor_.view;
            resolvedColorInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            asInfos[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asInfos[i].accelerationStructureCount = 1;
            asInfos[i].pAccelerationStructures = &v5Tlas_.handle;

            writes[i][0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][0].dstSet = v5DescriptorSets_[i];
            writes[i][0].dstBinding = 0;
            writes[i][0].descriptorCount = 1;
            writes[i][0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i][0].pBufferInfo = &bufferInfo;

            writes[i][1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][1].dstSet = v5DescriptorSets_[i];
            writes[i][1].dstBinding = 1;
            writes[i][1].descriptorCount = 1;
            writes[i][1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][1].pImageInfo = &outputInfos[i];

            writes[i][2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][2].dstSet = v5DescriptorSets_[i];
            writes[i][2].dstBinding = 5;
            writes[i][2].descriptorCount = 1;
            writes[i][2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            writes[i][2].pImageInfo = &samplerInfo;

            writes[i][3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][3].dstSet = v5DescriptorSets_[i];
            writes[i][3].dstBinding = kGBufferAlbedoBinding;
            writes[i][3].descriptorCount = 1;
            writes[i][3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][3].pImageInfo = &albedoInfo;

            writes[i][4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][4].dstSet = v5DescriptorSets_[i];
            writes[i][4].dstBinding = kGBufferNormalBinding;
            writes[i][4].descriptorCount = 1;
            writes[i][4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][4].pImageInfo = &normalInfo;

            writes[i][5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][5].dstSet = v5DescriptorSets_[i];
            writes[i][5].dstBinding = kGBufferWorldBinding;
            writes[i][5].descriptorCount = 1;
            writes[i][5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][5].pImageInfo = &worldInfo;

            writes[i][6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][6].pNext = &asInfos[i];
            writes[i][6].dstSet = v5DescriptorSets_[i];
            writes[i][6].dstBinding = kV5SceneTlasBinding;
            writes[i][6].descriptorCount = 1;
            writes[i][6].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            writes[i][7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][7].dstSet = v5DescriptorSets_[i];
            writes[i][7].dstBinding = kV5HistoryInputBinding;
            writes[i][7].descriptorCount = 1;
            writes[i][7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][7].pImageInfo = &historyInputInfos[i];

            writes[i][8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][8].dstSet = v5DescriptorSets_[i];
            writes[i][8].dstBinding = kV5HistoryOutputBinding;
            writes[i][8].descriptorCount = 1;
            writes[i][8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][8].pImageInfo = &historyOutputInfos[i];

            writes[i][9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][9].dstSet = v5DescriptorSets_[i];
            writes[i][9].dstBinding = kV4ManyLightBufferBinding;
            writes[i][9].descriptorCount = 1;
            writes[i][9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i][9].pBufferInfo = &lightInfo;

            writes[i][10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][10].dstSet = v5DescriptorSets_[i];
            writes[i][10].dstBinding = kV5ShadowSignalBinding;
            writes[i][10].descriptorCount = 1;
            writes[i][10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][10].pImageInfo = &shadowSignalInfos[i];

            writes[i][11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][11].dstSet = v5DescriptorSets_[i];
            writes[i][11].dstBinding = kV5ReflectionSignalBinding;
            writes[i][11].descriptorCount = 1;
            writes[i][11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][11].pImageInfo = &reflectionSignalInfos[i];

            writes[i][12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][12].dstSet = v5DescriptorSets_[i];
            writes[i][12].dstBinding = kV5ShadowHistoryInputBinding;
            writes[i][12].descriptorCount = 1;
            writes[i][12].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][12].pImageInfo = &shadowHistoryInputInfos[i];

            writes[i][13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][13].dstSet = v5DescriptorSets_[i];
            writes[i][13].dstBinding = kV5ShadowHistoryOutputBinding;
            writes[i][13].descriptorCount = 1;
            writes[i][13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][13].pImageInfo = &shadowHistoryOutputInfos[i];

            writes[i][14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][14].dstSet = v5DescriptorSets_[i];
            writes[i][14].dstBinding = kV5ReflectionHistoryInputBinding;
            writes[i][14].descriptorCount = 1;
            writes[i][14].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][14].pImageInfo = &reflectionHistoryInputInfos[i];

            writes[i][15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][15].dstSet = v5DescriptorSets_[i];
            writes[i][15].dstBinding = kV5ReflectionHistoryOutputBinding;
            writes[i][15].descriptorCount = 1;
            writes[i][15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][15].pImageInfo = &reflectionHistoryOutputInfos[i];

            writes[i][16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][16].dstSet = v5DescriptorSets_[i];
            writes[i][16].dstBinding = kV5SurfaceHistoryInputBinding;
            writes[i][16].descriptorCount = 1;
            writes[i][16].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i][16].pImageInfo = &surfaceHistoryInputInfos[i];

            writes[i][17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][17].dstSet = v5DescriptorSets_[i];
            writes[i][17].dstBinding = kV5SurfaceHistoryOutputBinding;
            writes[i][17].descriptorCount = 1;
            writes[i][17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][17].pImageInfo = &surfaceHistoryOutputInfos[i];

            writes[i][18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i][18].dstSet = v5DescriptorSets_[i];
            writes[i][18].dstBinding = kV5ResolvedColorBinding;
            writes[i][18].descriptorCount = 1;
            writes[i][18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i][18].pImageInfo = &resolvedColorInfos[i];
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes[i].size()), writes[i].data(), 0, nullptr);
        }
    }

    void createCommands() {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = queueFamilies_.graphics;
        require(vkCreateCommandPool(device_, &pool, nullptr, &commandPool_), "vkCreateCommandPool");

        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = commandPool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        require(vkAllocateCommandBuffers(device_, &allocate, &commandBuffer_), "vkAllocateCommandBuffers");
    }

    void createSync() {
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &imageAvailable_), "vkCreateSemaphore");
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &renderFinished_), "vkCreateSemaphore");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        require(vkCreateFence(device_, &fence, nullptr, &inFlight_), "vkCreateFence");
    }

    bool cameraChangedForHistory(const V1CameraSettings& camera) const {
        if (!hasLastV5Camera_) {
            return true;
        }
        const auto changed = [](float a, float b, float epsilon) {
            return std::abs(a - b) > epsilon;
        };
        return changed(camera.eyeX, lastV5Camera_.eyeX, 0.0005f)
            || changed(camera.eyeY, lastV5Camera_.eyeY, 0.0005f)
            || changed(camera.eyeZ, lastV5Camera_.eyeZ, 0.0005f)
            || changed(camera.targetX, lastV5Camera_.targetX, 0.0005f)
            || changed(camera.targetY, lastV5Camera_.targetY, 0.0005f)
            || changed(camera.targetZ, lastV5Camera_.targetZ, 0.0005f)
            || changed(camera.upX, lastV5Camera_.upX, 0.0005f)
            || changed(camera.upY, lastV5Camera_.upY, 0.0005f)
            || changed(camera.upZ, lastV5Camera_.upZ, 0.0005f)
            || changed(camera.fovY, lastV5Camera_.fovY, 0.0005f);
    }

    static float halton(std::uint32_t index, std::uint32_t base) {
        float result = 0.0f;
        float fraction = 1.0f / static_cast<float>(base);
        while (index > 0) {
            result += static_cast<float>(index % base) * fraction;
            index /= base;
            fraction /= static_cast<float>(base);
        }
        return result;
    }

    void updateUniform(const V1CameraSettings& cameraSettings, std::uint32_t v4DebugMode) {
        const V1CameraSettings previousCamera = hasLastV5Camera_ ? lastV5Camera_ : cameraSettings;
        const std::array<float, 2> previousJitter = lastV5Jitter_;
        const Vec3 eye = eyeOf(cameraSettings);
        const Vec3 forward = normalize(targetOf(cameraSettings) - eye);
        const Vec3 right = rightFor(forward, upOf(cameraSettings));
        const Vec3 up = normalize(cross(right, forward));
        const Vec3 prevEye = eyeOf(previousCamera);
        const Vec3 prevForward = normalize(targetOf(previousCamera) - prevEye);
        const Vec3 prevRight = rightFor(prevForward, upOf(previousCamera));
        const Vec3 prevUp = normalize(cross(prevRight, prevForward));
        const float nearPlane = cameraSettings.nearPlane > 0.0f ? cameraSettings.nearPlane : 0.05f;
        const float farPlane = cameraSettings.farPlane > 0.0f ? cameraSettings.farPlane : 200.0f;
        const float fovY = cameraSettings.fovY > 0.0f ? cameraSettings.fovY : (52.0f * kPi / 180.0f);
        const float aspect = static_cast<float>(swapchainExtent_.width) / static_cast<float>(swapchainExtent_.height);
        const VkExtent2D renderExtent = enableV5RayTracing_ ? v5RenderExtent() : swapchainExtent_;
        const float prevNearPlane = previousCamera.nearPlane > 0.0f ? previousCamera.nearPlane : 0.05f;
        const float prevFarPlane = previousCamera.farPlane > 0.0f ? previousCamera.farPlane : 200.0f;
        const float prevFovY = previousCamera.fovY > 0.0f ? previousCamera.fovY : (52.0f * kPi / 180.0f);

        CameraUniform uniform{};
        uniform.eyeNear[0] = eye.x;
        uniform.eyeNear[1] = eye.y;
        uniform.eyeNear[2] = eye.z;
        uniform.eyeNear[3] = nearPlane;
        uniform.rightFar[0] = right.x;
        uniform.rightFar[1] = right.y;
        uniform.rightFar[2] = right.z;
        uniform.rightFar[3] = farPlane;
        uniform.upTanHalf[0] = up.x;
        uniform.upTanHalf[1] = up.y;
        uniform.upTanHalf[2] = up.z;
        uniform.upTanHalf[3] = std::tan(fovY * 0.5f);
        uniform.forwardAspect[0] = forward.x;
        uniform.forwardAspect[1] = forward.y;
        uniform.forwardAspect[2] = forward.z;
        uniform.forwardAspect[3] = aspect;

        const Vec3 sunToScene = normalize({0.28f, 0.44f, -0.85f});
        Vec3 shadowRight = normalize(cross({0.0f, 0.0f, 1.0f}, sunToScene));
        if (dot(shadowRight, shadowRight) <= 0.00001f) {
            shadowRight = {1.0f, 0.0f, 0.0f};
        }
        const Vec3 shadowUp = normalize(cross(sunToScene, shadowRight));
        const Vec3 shadowCenter{0.0f, 0.0f, 0.65f};
        uniform.shadowRightExtent[0] = shadowRight.x;
        uniform.shadowRightExtent[1] = shadowRight.y;
        uniform.shadowRightExtent[2] = shadowRight.z;
        uniform.shadowRightExtent[3] = 5.6f;
        uniform.shadowUpNear[0] = shadowUp.x;
        uniform.shadowUpNear[1] = shadowUp.y;
        uniform.shadowUpNear[2] = shadowUp.z;
        uniform.shadowUpNear[3] = -4.8f;
        uniform.shadowForwardFar[0] = sunToScene.x;
        uniform.shadowForwardFar[1] = sunToScene.y;
        uniform.shadowForwardFar[2] = sunToScene.z;
        uniform.shadowForwardFar[3] = 6.8f;
        uniform.shadowCenterBias[0] = shadowCenter.x;
        uniform.shadowCenterBias[1] = shadowCenter.y;
        uniform.shadowCenterBias[2] = shadowCenter.z;
        uniform.shadowCenterBias[3] = 0.0025f;
        uniform.pointPosRadius[0] = 2.8f;
        uniform.pointPosRadius[1] = -1.8f;
        uniform.pointPosRadius[2] = 2.6f;
        uniform.pointPosRadius[3] = 4.8f;
        uniform.pointColorIntensity[0] = 1.0f;
        uniform.pointColorIntensity[1] = 0.62f;
        uniform.pointColorIntensity[2] = 0.30f;
        uniform.pointColorIntensity[3] = 1.25f;
        const Vec3 spotDir = normalize({0.18f, 0.72f, -0.67f});
        uniform.spotPosInner[0] = -0.6f;
        uniform.spotPosInner[1] = -4.4f;
        uniform.spotPosInner[2] = 4.8f;
        uniform.spotPosInner[3] = std::cos(20.0f * kPi / 180.0f);
        uniform.spotDirOuter[0] = spotDir.x;
        uniform.spotDirOuter[1] = spotDir.y;
        uniform.spotDirOuter[2] = spotDir.z;
        uniform.spotDirOuter[3] = std::cos(42.0f * kPi / 180.0f);
        uniform.spotColorIntensity[0] = 0.45f;
        uniform.spotColorIntensity[1] = 0.68f;
        uniform.spotColorIntensity[2] = 1.0f;
        uniform.spotColorIntensity[3] = 0.85f;
        uniform.v3Flags[0] = enableV4Ssao_ ? 2.0f : (enableV3Shadows_ ? 1.0f : 0.0f);
        uniform.v3Flags[1] = 2.4f;
        uniform.v3Flags[2] = 4.5f;
        uniform.v3Flags[3] = 7.2f;
        uniform.v4Flags[0] = geometry_.manyLightDemo ? 1.0f : 0.0f;
        uniform.v4Flags[1] = static_cast<float>(lightCount_);
        uniform.v4Flags[2] = static_cast<float>(v4DebugMode);
        if (enableV5RayTracing_) {
            const bool cameraChanged = cameraChangedForHistory(cameraSettings);
            if (!hasLastV5Camera_) {
                v5HistoryFrameCount_ = 0;
            } else if (cameraChanged) {
                v5HistoryFrameCount_ = std::min<std::uint32_t>(v5HistoryFrameCount_ + 1u, 8u);
            } else {
                v5HistoryFrameCount_ = std::min<std::uint32_t>(v5HistoryFrameCount_ + 1u, 240u);
            }
            lastV5Camera_ = cameraSettings;
            hasLastV5Camera_ = true;
            uniform.v4Flags[3] = static_cast<float>(v5HistoryFrameCount_);

            const std::uint32_t jitterIndex = (frameIndex_ % 16u) + 1u;
            const float jitterPixelX = halton(jitterIndex, 2u) - 0.5f;
            const float jitterPixelY = halton(jitterIndex, 3u) - 0.5f;
            uniform.taaJitter[0] = (2.0f * jitterPixelX) / static_cast<float>(std::max<std::uint32_t>(renderExtent.width, 1u));
            uniform.taaJitter[1] = (2.0f * jitterPixelY) / static_cast<float>(std::max<std::uint32_t>(renderExtent.height, 1u));
            uniform.taaJitter[2] = jitterPixelX;
            uniform.taaJitter[3] = jitterPixelY;
            uniform.prevEyeNear[0] = prevEye.x;
            uniform.prevEyeNear[1] = prevEye.y;
            uniform.prevEyeNear[2] = prevEye.z;
            uniform.prevEyeNear[3] = prevNearPlane;
            uniform.prevRightFar[0] = prevRight.x;
            uniform.prevRightFar[1] = prevRight.y;
            uniform.prevRightFar[2] = prevRight.z;
            uniform.prevRightFar[3] = prevFarPlane;
            uniform.prevUpTanHalf[0] = prevUp.x;
            uniform.prevUpTanHalf[1] = prevUp.y;
            uniform.prevUpTanHalf[2] = prevUp.z;
            uniform.prevUpTanHalf[3] = std::tan(prevFovY * 0.5f);
            uniform.prevForwardAspect[0] = prevForward.x;
            uniform.prevForwardAspect[1] = prevForward.y;
            uniform.prevForwardAspect[2] = prevForward.z;
            uniform.prevForwardAspect[3] = aspect;
            uniform.prevTaaJitter[0] = (2.0f * previousJitter[0]) / static_cast<float>(std::max<std::uint32_t>(renderExtent.width, 1u));
            uniform.prevTaaJitter[1] = (2.0f * previousJitter[1]) / static_cast<float>(std::max<std::uint32_t>(renderExtent.height, 1u));
            uniform.prevTaaJitter[2] = previousJitter[0];
            uniform.prevTaaJitter[3] = previousJitter[1];
            lastV5Jitter_ = {jitterPixelX, jitterPixelY};
        } else {
            uniform.v4Flags[3] = static_cast<float>(frameIndex_);
        }

        void* mapped = nullptr;
        require(vkMapMemory(device_, uniformMemory_, 0, sizeof(uniform), 0, &mapped), "vkMapMemory(uniform)");
        std::memcpy(mapped, &uniform, sizeof(uniform));
        vkUnmapMemory(device_, uniformMemory_);
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer");

        if (enableV3Shadows_) {
            VkClearValue shadowClear{};
            shadowClear.depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo shadowPass{};
            shadowPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPass.renderPass = shadowRenderPass_;
            shadowPass.framebuffer = shadowFramebuffer_;
            shadowPass.renderArea.extent = {kShadowAtlasWidth, kShadowAtlasHeight};
            shadowPass.clearValueCount = 1;
            shadowPass.pClearValues = &shadowClear;
            vkCmdBeginRenderPass(commandBuffer, &shadowPass, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
            const VkDescriptorSet shadowDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &shadowDescriptorSet, 0, nullptr);
            VkDeviceSize shadowOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &shadowOffset);
            for (std::uint32_t shadowIndex = 0; shadowIndex < kShadowMapCount; ++shadowIndex) {
                const std::uint32_t col = shadowIndex % kShadowAtlasColumns;
                const std::uint32_t row = shadowIndex / kShadowAtlasColumns;
                VkViewport shadowViewport{};
                shadowViewport.x = static_cast<float>(col * kShadowTileSize);
                shadowViewport.y = static_cast<float>(row * kShadowTileSize);
                shadowViewport.width = static_cast<float>(kShadowTileSize);
                shadowViewport.height = static_cast<float>(kShadowTileSize);
                shadowViewport.minDepth = 0.0f;
                shadowViewport.maxDepth = 1.0f;
                VkRect2D shadowScissor{};
                shadowScissor.offset = {static_cast<std::int32_t>(col * kShadowTileSize), static_cast<std::int32_t>(row * kShadowTileSize)};
                shadowScissor.extent = {kShadowTileSize, kShadowTileSize};
                vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
                vkCmdDraw(commandBuffer, vertexCount_, 1, 0, shadowIndex);
            }
            vkCmdEndRenderPass(commandBuffer);

        }

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent = swapchainExtent_;

        if (enableV5RayTracing_) {
            const VkExtent2D internalExtent = v5RenderExtent();
            VkViewport internalViewport{};
            internalViewport.width = static_cast<float>(internalExtent.width);
            internalViewport.height = static_cast<float>(internalExtent.height);
            internalViewport.minDepth = 0.0f;
            internalViewport.maxDepth = 1.0f;
            VkRect2D internalScissor{};
            internalScissor.extent = internalExtent;

            if (frameIndex_ < 4) previewLog("record v5: begin gbuffer pass");
            std::array<VkClearValue, 4> gbufferClears{};
            gbufferClears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};
            gbufferClears[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[3].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo gbufferPass{};
            gbufferPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            gbufferPass.renderPass = gbufferRenderPass_;
            gbufferPass.framebuffer = gbufferFramebuffer_;
            gbufferPass.renderArea.extent = internalExtent;
            gbufferPass.clearValueCount = static_cast<std::uint32_t>(gbufferClears.size());
            gbufferPass.pClearValues = gbufferClears.data();
            vkCmdBeginRenderPass(commandBuffer, &gbufferPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &internalViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &internalScissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline_);
            VkDeviceSize gbufferOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &gbufferOffset);
            const VkDescriptorSet fallbackDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
            if (geometry_.batches.empty()) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
                vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
            } else {
                if (frameIndex_ < 4) previewLog("record v5: draw batches");
                for (const GpuPreviewGeometry::Batch& batch : geometry_.batches) {
                    if (batch.vertexCount == 0) {
                        continue;
                    }
                    const std::uint32_t materialIndex = batch.materialIndex < descriptorSets_.size() ? batch.materialIndex : 0u;
                    const VkDescriptorSet descriptorSet = descriptorSets_[materialIndex];
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
                    vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
                }
            }
            if (sphereVertexCount_ > 0 && sphereInstanceCount_ > 0) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedGBufferPipeline_);
                const VkDescriptorSet instancedDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &instancedDescriptorSet, 0, nullptr);
                std::array<VkBuffer, 2> sphereBuffers{sphereVertexBuffer_, sphereInstanceBuffer_};
                std::array<VkDeviceSize, 2> sphereOffsets{0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<std::uint32_t>(sphereBuffers.size()), sphereBuffers.data(), sphereOffsets.data());
                vkCmdDraw(commandBuffer, sphereVertexCount_, sphereInstanceCount_, 0, 0);
            }
            if (frameIndex_ < 4) previewLog("record v5: end gbuffer pass");
            vkCmdEndRenderPass(commandBuffer);

            if (frameIndex_ < 4) previewLog("record v5: gbuffer compute barrier");
            std::array<VkImageMemoryBarrier, 3> gbufferReadBarriers{};
            for (std::size_t i = 0; i < gbufferReadBarriers.size(); ++i) {
                VkImageMemoryBarrier& barrier = gbufferReadBarriers[i];
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = gbufferTargets_[i].image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            }
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<std::uint32_t>(gbufferReadBarriers.size()),
                gbufferReadBarriers.data()
            );

            const std::uint32_t historyWriteIndex = frameIndex_ & 1u;
            const std::uint32_t historyReadIndex = 1u - historyWriteIndex;
            std::array<VkImageMemoryBarrier, 11> historyBeforeCompute{};
            auto setHistoryReadBarrier = [](VkImageMemoryBarrier& barrier, VkImage image, bool initialized) {
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            };
            auto setHistoryWriteBarrier = [](VkImageMemoryBarrier& barrier, VkImage image, bool initialized) {
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            };
            auto setSignalWriteBarrier = [](VkImageMemoryBarrier& barrier, VkImage image, bool initialized) {
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = initialized ? VK_ACCESS_SHADER_WRITE_BIT : 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            };
            setHistoryReadBarrier(historyBeforeCompute[0], v5HistoryTargets_[historyReadIndex].image, v5HistoryInitialized_[historyReadIndex]);
            setHistoryWriteBarrier(historyBeforeCompute[1], v5HistoryTargets_[historyWriteIndex].image, v5HistoryInitialized_[historyWriteIndex]);
            setHistoryReadBarrier(historyBeforeCompute[2], v5ShadowHistoryTargets_[historyReadIndex].image, v5ShadowHistoryInitialized_[historyReadIndex]);
            setHistoryWriteBarrier(historyBeforeCompute[3], v5ShadowHistoryTargets_[historyWriteIndex].image, v5ShadowHistoryInitialized_[historyWriteIndex]);
            setHistoryReadBarrier(historyBeforeCompute[4], v5ReflectionHistoryTargets_[historyReadIndex].image, v5ReflectionHistoryInitialized_[historyReadIndex]);
            setHistoryWriteBarrier(historyBeforeCompute[5], v5ReflectionHistoryTargets_[historyWriteIndex].image, v5ReflectionHistoryInitialized_[historyWriteIndex]);
            setHistoryReadBarrier(historyBeforeCompute[6], v5SurfaceHistoryTargets_[historyReadIndex].image, v5SurfaceHistoryInitialized_[historyReadIndex]);
            setHistoryWriteBarrier(historyBeforeCompute[7], v5SurfaceHistoryTargets_[historyWriteIndex].image, v5SurfaceHistoryInitialized_[historyWriteIndex]);
            setSignalWriteBarrier(historyBeforeCompute[8], v5ShadowSignal_.image, v5SignalInitialized_);
            setSignalWriteBarrier(historyBeforeCompute[9], v5ReflectionSignal_.image, v5SignalInitialized_);
            setSignalWriteBarrier(historyBeforeCompute[10], v5ResolvedColor_.image, v5ResolvedColorInitialized_);
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<std::uint32_t>(historyBeforeCompute.size()),
                historyBeforeCompute.data()
            );

            if (frameIndex_ < 4) previewLog("record v5: swapchain to general");
            VkImageMemoryBarrier toGeneral{};
            toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            const bool firstSwapchainUse = imageIndex >= v5SwapchainImageInitialized_.size() || !v5SwapchainImageInitialized_[imageIndex];
            toGeneral.oldLayout = firstSwapchainUse ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.image = swapchainImages_[imageIndex];
            toGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            toGeneral.subresourceRange.levelCount = 1;
            toGeneral.subresourceRange.layerCount = 1;
            toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toGeneral
            );

            if (frameIndex_ < 4) previewLog("record v5: dispatch ray signal compute");
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5RayTracingPipeline_);
            const std::size_t descriptorIndex = static_cast<std::size_t>(imageIndex) * 2u + historyWriteIndex;
            const VkDescriptorSet descriptorSet = v5DescriptorSets_[descriptorIndex];
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5RayTracingPipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, (internalExtent.width + 7u) / 8u, (internalExtent.height + 7u) / 8u, 1);

            std::array<VkImageMemoryBarrier, 2> signalAfterRay{};
            auto setSignalReadBarrier = [](VkImageMemoryBarrier& barrier, VkImage image) {
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            };
            setSignalReadBarrier(signalAfterRay[0], v5ShadowSignal_.image);
            setSignalReadBarrier(signalAfterRay[1], v5ReflectionSignal_.image);
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<std::uint32_t>(signalAfterRay.size()),
                signalAfterRay.data()
            );

            if (frameIndex_ < 4) previewLog("record v5: dispatch denoise compute");
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5DenoisePipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5RayTracingPipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, (internalExtent.width + 7u) / 8u, (internalExtent.height + 7u) / 8u, 1);

            std::array<VkImageMemoryBarrier, 4> historyAfterCompute{};
            auto setHistoryAfterCompute = [](VkImageMemoryBarrier& barrier, VkImage image) {
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            };
            setHistoryAfterCompute(historyAfterCompute[0], v5HistoryTargets_[historyWriteIndex].image);
            setHistoryAfterCompute(historyAfterCompute[1], v5ShadowHistoryTargets_[historyWriteIndex].image);
            setHistoryAfterCompute(historyAfterCompute[2], v5ReflectionHistoryTargets_[historyWriteIndex].image);
            setHistoryAfterCompute(historyAfterCompute[3], v5SurfaceHistoryTargets_[historyWriteIndex].image);
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<std::uint32_t>(historyAfterCompute.size()),
                historyAfterCompute.data()
            );
            v5HistoryInitialized_[historyWriteIndex] = true;
            v5ShadowHistoryInitialized_[historyWriteIndex] = true;
            v5ReflectionHistoryInitialized_[historyWriteIndex] = true;
            v5SurfaceHistoryInitialized_[historyWriteIndex] = true;
            v5SignalInitialized_ = true;
            v5ResolvedColorInitialized_ = true;

            VkImageMemoryBarrier resolvedBeforeDownsample{};
            resolvedBeforeDownsample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            resolvedBeforeDownsample.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            resolvedBeforeDownsample.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            resolvedBeforeDownsample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolvedBeforeDownsample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            resolvedBeforeDownsample.image = v5ResolvedColor_.image;
            resolvedBeforeDownsample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolvedBeforeDownsample.subresourceRange.levelCount = 1;
            resolvedBeforeDownsample.subresourceRange.layerCount = 1;
            resolvedBeforeDownsample.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            resolvedBeforeDownsample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &resolvedBeforeDownsample
            );

            if (frameIndex_ < 4) previewLog("record v5: dispatch downsample compute");
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5DownsamplePipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, v5RayTracingPipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, (swapchainExtent_.width + 7u) / 8u, (swapchainExtent_.height + 7u) / 8u, 1);

            if (frameIndex_ < 4) previewLog("record v5: swapchain to present");
            VkImageMemoryBarrier toPresent = toGeneral;
            toPresent.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            toPresent.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            toPresent.dstAccessMask = 0;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toPresent
            );

            if (frameIndex_ < 4) previewLog("record v5: end command buffer");
            require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
            if (imageIndex < v5SwapchainImageInitialized_.size()) {
                v5SwapchainImageInitialized_[imageIndex] = true;
            }
            return;
        }

        if (enableV4Ssao_) {
            std::array<VkClearValue, 4> gbufferClears{};
            gbufferClears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};
            gbufferClears[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[3].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo gbufferPass{};
            gbufferPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            gbufferPass.renderPass = gbufferRenderPass_;
            gbufferPass.framebuffer = gbufferFramebuffer_;
            gbufferPass.renderArea.extent = swapchainExtent_;
            gbufferPass.clearValueCount = static_cast<std::uint32_t>(gbufferClears.size());
            gbufferPass.pClearValues = gbufferClears.data();
            vkCmdBeginRenderPass(commandBuffer, &gbufferPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline_);
            VkDeviceSize gbufferOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &gbufferOffset);
            const VkDescriptorSet fallbackDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
            if (geometry_.batches.empty()) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
                vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
            } else {
                for (const GpuPreviewGeometry::Batch& batch : geometry_.batches) {
                    if (batch.vertexCount == 0) {
                        continue;
                    }
                    const std::uint32_t materialIndex = batch.materialIndex < descriptorSets_.size() ? batch.materialIndex : 0u;
                    const VkDescriptorSet descriptorSet = descriptorSets_[materialIndex];
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
                    vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
                }
            }
            if (sphereVertexCount_ > 0 && sphereInstanceCount_ > 0) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedGBufferPipeline_);
                const VkDescriptorSet instancedDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &instancedDescriptorSet, 0, nullptr);
                std::array<VkBuffer, 2> sphereBuffers{sphereVertexBuffer_, sphereInstanceBuffer_};
                std::array<VkDeviceSize, 2> sphereOffsets{0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<std::uint32_t>(sphereBuffers.size()), sphereBuffers.data(), sphereOffsets.data());
                vkCmdDraw(commandBuffer, sphereVertexCount_, sphereInstanceCount_, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            VkClearValue ssaoClear{};
            ssaoClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
            VkRenderPassBeginInfo ssaoPass{};
            ssaoPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            ssaoPass.renderPass = ssaoRenderPass_;
            ssaoPass.framebuffer = ssaoFramebuffer_;
            ssaoPass.renderArea.extent = swapchainExtent_;
            ssaoPass.clearValueCount = 1;
            ssaoPass.pClearValues = &ssaoClear;
            vkCmdBeginRenderPass(commandBuffer, &ssaoPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            if (!geometry_.manyLightDemo) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline_);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            VkClearValue blurClear{};
            blurClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
            VkRenderPassBeginInfo blurPass{};
            blurPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            blurPass.renderPass = ssaoRenderPass_;
            blurPass.framebuffer = ssaoBlurFramebuffer_;
            blurPass.renderArea.extent = swapchainExtent_;
            blurPass.clearValueCount = 1;
            blurPass.pClearValues = &blurClear;
            vkCmdBeginRenderPass(commandBuffer, &blurPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            if (!geometry_.manyLightDemo) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoBlurPipeline_);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            std::array<VkClearValue, 2> composeClears{};
            composeClears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
            composeClears[1].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo composePass{};
            composePass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            composePass.renderPass = renderPass_;
            composePass.framebuffer = framebuffers_[imageIndex];
            composePass.renderArea.extent = swapchainExtent_;
            composePass.clearValueCount = static_cast<std::uint32_t>(composeClears.size());
            composePass.pClearValues = composeClears.data();
            vkCmdBeginRenderPass(commandBuffer, &composePass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffer);
            require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
            return;
        }

        std::array<VkClearValue, 2> clears{};
        clears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPass{};
        renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPass.renderPass = renderPass_;
        renderPass.framebuffer = framebuffers_[imageIndex];
        renderPass.renderArea.extent = swapchainExtent_;
        renderPass.clearValueCount = static_cast<std::uint32_t>(clears.size());
        renderPass.pClearValues = clears.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPass, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        const VkDescriptorSet fallbackDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &offset);
        if (geometry_.batches.empty()) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
            vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
        } else {
            for (const GpuPreviewGeometry::Batch& batch : geometry_.batches) {
                if (batch.vertexCount == 0) {
                    continue;
                }
                const std::uint32_t materialIndex = batch.materialIndex < descriptorSets_.size() ? batch.materialIndex : 0u;
                const VkDescriptorSet descriptorSet = descriptorSets_[materialIndex];
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
                vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
            }
        }
        vkCmdEndRenderPass(commandBuffer);
        require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
    }

    HWND hwnd_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    const GpuPreviewGeometry& geometry_;
    bool enableV3Shadows_ = false;
    bool enableV5RayTracing_ = false;
    std::string gpuName_;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    QueueFamilies queueFamilies_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<bool> v5SwapchainImageInitialized_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass gbufferRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass ssaoRenderPass_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkImage msaaColorImage_ = VK_NULL_HANDLE;
    VkDeviceMemory msaaColorMemory_ = VK_NULL_HANDLE;
    VkImageView msaaColorView_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    VkImage shadowDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowDepthMemory_ = VK_NULL_HANDLE;
    VkImageView shadowDepthView_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
    std::array<TextureResource, 3> gbufferTargets_{};
    std::array<TextureResource, 3> gbufferMsColor_{};
    VkSampleCountFlagBits gbufferSamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkImage gbufferDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory gbufferDepthMemory_ = VK_NULL_HANDLE;
    VkImageView gbufferDepthView_ = VK_NULL_HANDLE;
    VkFramebuffer gbufferFramebuffer_ = VK_NULL_HANDLE;
    TextureResource ssaoTarget_{};
    TextureResource ssaoBlurTarget_{};
    std::array<TextureResource, 2> v5HistoryTargets_{};
    std::array<bool, 2> v5HistoryInitialized_{false, false};
    TextureResource v5ShadowSignal_{};
    TextureResource v5ReflectionSignal_{};
    std::array<TextureResource, 2> v5ShadowHistoryTargets_{};
    std::array<TextureResource, 2> v5ReflectionHistoryTargets_{};
    std::array<TextureResource, 2> v5SurfaceHistoryTargets_{};
    TextureResource v5ResolvedColor_{};
    std::array<bool, 2> v5ShadowHistoryInitialized_{false, false};
    std::array<bool, 2> v5ReflectionHistoryInitialized_{false, false};
    std::array<bool, 2> v5SurfaceHistoryInitialized_{false, false};
    bool v5SignalInitialized_ = false;
    bool v5ResolvedColorInitialized_ = false;
    VkFramebuffer ssaoFramebuffer_ = VK_NULL_HANDLE;
    VkFramebuffer ssaoBlurFramebuffer_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexBytes_ = 0;
    std::uint32_t vertexCount_ = 0;
    VkBuffer sphereVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sphereVertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize sphereVertexBytes_ = 0;
    std::uint32_t sphereVertexCount_ = 0;
    VkBuffer sphereInstanceBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sphereInstanceMemory_ = VK_NULL_HANDLE;
    VkDeviceSize sphereInstanceBytes_ = 0;
    std::uint32_t sphereInstanceCount_ = 0;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory_ = VK_NULL_HANDLE;
    VkBuffer lightBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory lightMemory_ = VK_NULL_HANDLE;
    VkDeviceSize lightBytes_ = 0;
    std::uint32_t lightCount_ = 0;
    AccelerationStructureResource v5Blas_{};
    AccelerationStructureResource v5Tlas_{};
    std::vector<MaterialTextureResources> materialTextures_;
    std::array<TextureResource, kSharedTextureCount> sharedTextures_{};
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    bool samplerAnisotropy_ = false;
    float maxSamplerAnisotropy_ = 1.0f;
    std::uint32_t maxTextureMipLevels_ = 1;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkDescriptorSetLayout v4DescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool v4DescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet v4DescriptorSet_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout v5DescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool v5DescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> v5DescriptorSets_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout v4ComposePipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout v5RayTracingPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline skyPipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline gbufferPipeline_ = VK_NULL_HANDLE;
    VkPipeline instancedGBufferPipeline_ = VK_NULL_HANDLE;
    VkPipeline ssaoPipeline_ = VK_NULL_HANDLE;
    VkPipeline ssaoBlurPipeline_ = VK_NULL_HANDLE;
    VkPipeline v4ComposePipeline_ = VK_NULL_HANDLE;
    VkPipeline v5RayTracingPipeline_ = VK_NULL_HANDLE;
    VkPipeline v5DenoisePipeline_ = VK_NULL_HANDLE;
    VkPipeline v5DownsamplePipeline_ = VK_NULL_HANDLE;
    VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;
    std::uint32_t frameIndex_ = 0;
    std::uint32_t v5HistoryFrameCount_ = 0;
    V1CameraSettings lastV5Camera_{};
    std::array<float, 2> lastV5Jitter_{0.0f, 0.0f};
    bool hasLastV5Camera_ = false;
    bool enableV4Ssao_ = false;
};

struct PreviewState {
    V1RenderSettings settings;
    GpuPreviewGeometry geometry;
    std::unique_ptr<VulkanGpuRenderer> renderer;
    std::array<bool, 256> keys{};
    V1CameraSettings camera;
    bool roaming = false;
    bool mouseLook = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint32_t v4DebugMode = 0;
    std::uint32_t frameIndex = 0;
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
};

PreviewState* stateFrom(HWND hwnd) {
    return reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void deriveAnglesFromCamera(PreviewState& state) {
    const Vec3 forward = normalize(targetOf(state.camera) - eyeOf(state.camera));
    state.yaw = std::atan2(forward.y, forward.x);
    state.pitch = std::asin(std::clamp(forward.z, -1.0f, 1.0f));
}

void writeCameraPose(PreviewState& state, Vec3 eye) {
    const Vec3 forward = forwardFor(state.yaw, state.pitch);
    const Vec3 sceneUp = upOf(state.geometry.camera);
    const Vec3 right = rightFor(forward, sceneUp);
    const Vec3 up = normalize(cross(right, forward));
    state.camera.enabled = true;
    state.camera.eyeX = eye.x;
    state.camera.eyeY = eye.y;
    state.camera.eyeZ = eye.z;
    state.camera.targetX = eye.x + forward.x;
    state.camera.targetY = eye.y + forward.y;
    state.camera.targetZ = eye.z + forward.z;
    state.camera.upX = up.x;
    state.camera.upY = up.y;
    state.camera.upZ = up.z;
}

bool keyDown(const PreviewState& state, int key) {
    return key >= 0 && key < static_cast<int>(state.keys.size()) && state.keys[static_cast<std::size_t>(key)];
}

int mouseXFrom(LPARAM lParam) {
    return static_cast<int>(static_cast<short>(LOWORD(lParam)));
}

int mouseYFrom(LPARAM lParam) {
    return static_cast<int>(static_cast<short>(HIWORD(lParam)));
}

void rotateCamera(PreviewState& state, float deltaX, float deltaY) {
    const float mouseSensitivity = 0.0045f;
    state.yaw -= deltaX * mouseSensitivity;
    state.pitch = std::clamp(state.pitch - deltaY * mouseSensitivity, -1.45f, 1.45f);
    writeCameraPose(state, eyeOf(state.camera));
}

void updateCamera(PreviewState& state, float dt) {
    if (!state.roaming) {
        state.camera = state.geometry.camera;
        return;
    }
    const Vec3 sceneUp = upOf(state.geometry.camera);
    const float lookSpeed = 1.85f;
    if (keyDown(state, VK_LEFT) || keyDown(state, 'J')) state.yaw += lookSpeed * dt;
    if (keyDown(state, VK_RIGHT) || keyDown(state, 'L')) state.yaw -= lookSpeed * dt;
    if (keyDown(state, VK_UP) || keyDown(state, 'I')) state.pitch = std::clamp(state.pitch + lookSpeed * dt, -1.45f, 1.45f);
    if (keyDown(state, VK_DOWN) || keyDown(state, 'K')) state.pitch = std::clamp(state.pitch - lookSpeed * dt, -1.45f, 1.45f);

    const Vec3 forward = forwardFor(state.yaw, state.pitch);
    const Vec3 right = rightFor(forward, sceneUp);
    const Vec3 up = sceneUp;
    Vec3 movement{};
    if (keyDown(state, 'W')) movement = movement + forward;
    if (keyDown(state, 'S')) movement = movement - forward;
    if (keyDown(state, 'D')) movement = movement + right;
    if (keyDown(state, 'A')) movement = movement - right;
    if (keyDown(state, 'E') || keyDown(state, VK_SPACE)) movement = movement + up;
    if (keyDown(state, 'Q') || keyDown(state, VK_CONTROL)) movement = movement - up;
    const float speed = (GetKeyState(VK_SHIFT) & 0x8000) ? 20.0f : 7.0f;
    if (dot(movement, movement) > 0.00001f) {
        movement = normalize(movement) * (speed * dt);
    }
    writeCameraPose(state, eyeOf(state.camera) + movement);
}

bool shouldAnimateGeometry(const PreviewState& state) {
    return !state.settings.enableV2Shading && state.settings.scenePath.extension() == ".s72";
}

void advanceAnimatedGeometry(PreviewState& state) {
    if (!shouldAnimateGeometry(state)) {
        return;
    }
    state.settings.frameIndex = state.frameIndex++;
    state.geometry = buildGpuPreviewGeometry(state.settings);
    state.renderer->updateGeometry(state.geometry);
}

LRESULT CALLBACK vulkanPreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        return 0;
    case WM_TIMER:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (!state->renderer) {
                return 0;
            }
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - state->lastTick).count();
            state->lastTick = now;
            try {
                advanceAnimatedGeometry(*state);
                updateCamera(*state, std::max(0.001f, static_cast<float>(elapsedMs) / 1000.0f));
                state->renderer->draw(state->camera, state->v4DebugMode);
            } catch (const std::exception& error) {
                std::cerr << "Vulkan preview draw failed: " << error.what() << '\n';
                DestroyWindow(hwnd);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam < 256) {
            if (PreviewState* state = stateFrom(hwnd)) {
                state->keys[static_cast<std::size_t>(wParam)] = true;
                if (wParam == 'R' && !(lParam & (1 << 30))) {
                    state->roaming = !state->roaming;
                    if (state->roaming) {
                        state->camera = state->geometry.camera;
                        deriveAnglesFromCamera(*state);
                        writeCameraPose(*state, eyeOf(state->camera));
                        SetWindowTextW(hwnd, L"VulkanRender GPU preview - roaming camera");
                        std::cout << "GPU camera roaming enabled. WASD move, Q/E vertical, arrows/IJKL or right-drag look, Shift fast.\n";
                    } else {
                        state->mouseLook = false;
                        ReleaseCapture();
                        state->camera = state->geometry.camera;
                        SetWindowTextW(hwnd, L"VulkanRender GPU preview - press R for roaming");
                        std::cout << "GPU camera roaming disabled.\n";
                    }
                }
                if (wParam >= '0' && wParam <= '5' && !(lParam & (1 << 30))) {
                    state->v4DebugMode = static_cast<std::uint32_t>(wParam - '0');
                    const wchar_t* titles[] = {
                        L"VulkanRender GPU preview - v4 final",
                        L"VulkanRender GPU preview - v4 albedo",
                        L"VulkanRender GPU preview - v4 normal",
                        L"VulkanRender GPU preview - v4 depth",
                        L"VulkanRender GPU preview - v4 SSAO raw",
                        L"VulkanRender GPU preview - v4 SSAO blur",
                    };
                    SetWindowTextW(hwnd, titles[state->v4DebugMode]);
                    std::cout << "V4 debug view " << state->v4DebugMode << " selected. 0 final, 1 albedo, 2 normal, 3 depth, 4 SSAO raw, 5 SSAO blur.\n";
                }
            }
        }
        return 0;
    case WM_RBUTTONDOWN:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (state->roaming) {
                state->mouseLook = true;
                state->lastMouseX = mouseXFrom(lParam);
                state->lastMouseY = mouseYFrom(lParam);
                SetCapture(hwnd);
            }
        }
        return 0;
    case WM_RBUTTONUP:
        if (PreviewState* state = stateFrom(hwnd)) {
            state->mouseLook = false;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (state->roaming && state->mouseLook) {
                const int x = mouseXFrom(lParam);
                const int y = mouseYFrom(lParam);
                rotateCamera(*state, static_cast<float>(x - state->lastMouseX), static_cast<float>(y - state->lastMouseY));
                state->lastMouseX = x;
                state->lastMouseY = y;
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256) {
            if (PreviewState* state = stateFrom(hwnd)) {
                state->keys[static_cast<std::size_t>(wParam)] = false;
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kWindowTimer);
        if (PreviewState* state = stateFrom(hwnd)) {
            state->mouseLook = false;
        }
        ReleaseCapture();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} // namespace

int runVulkanPreviewWindow(V1RenderSettings settings) {
    resetPreviewLog();
    previewLog("runVulkanPreviewWindow: begin");
    PreviewState state;
    state.settings = std::move(settings);
    previewLog("runVulkanPreviewWindow: build geometry");
    state.geometry = buildGpuPreviewGeometry(state.settings);
    previewLog("runVulkanPreviewWindow: geometry vertices=" + std::to_string(state.geometry.vertices.size()));
    state.camera = state.geometry.camera;
    deriveAnglesFromCamera(state);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"LZJUVulkanRenderGpuPreview";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = vulkanPreviewProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    RECT rect{0, 0, static_cast<LONG>(state.settings.width), static_cast<LONG>(state.settings.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"VulkanRender GPU preview - R roaming, RMB drag look",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        &state
    );
    if (!hwnd) {
        std::cerr << "Could not create Vulkan preview window.\n";
        previewLog("runVulkanPreviewWindow: CreateWindowExW failed");
        return 1;
    }

    try {
        previewLog("runVulkanPreviewWindow: create renderer");
        state.renderer = std::make_unique<VulkanGpuRenderer>(hwnd, state.settings.width, state.settings.height, state.geometry, state.settings.enableV3Shadows, state.settings.enableV4Ssao, state.settings.enableV5RayTracing);
    } catch (const std::exception& error) {
        std::cerr << "Could not initialize Vulkan GPU preview: " << error.what() << '\n';
        previewLog(std::string("runVulkanPreviewWindow: renderer init failed: ") + error.what());
        DestroyWindow(hwnd);
        return 1;
    }
    previewLog("runVulkanPreviewWindow: timer start");
    SetTimer(hwnd, kWindowTimer, 16, nullptr);

    std::cout << "Opened Vulkan GPU preview window. vertices=" << state.geometry.vertices.size()
              << ". Press R for roaming, hold right mouse to look, Esc to exit.\n";

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    state.renderer.reset();
    return 0;
}

} // namespace vr
