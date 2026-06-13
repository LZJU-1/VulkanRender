#include "platform/VulkanPreviewWindow.hpp"

#include <stb_image.h>

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>

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

constexpr float kPi = 3.14159265358979323846f;
constexpr int kWindowTimer = 1;

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkCreateInstance vkCreateInstance = nullptr;
PFN_vkDestroyInstance vkDestroyInstance = nullptr;
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = nullptr;
PFN_vkCreateDevice vkCreateDevice = nullptr;
PFN_vkDestroyDevice vkDestroyDevice = nullptr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
PFN_vkCreateImageView vkCreateImageView = nullptr;
PFN_vkDestroyImageView vkDestroyImageView = nullptr;
PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
PFN_vkCreateBuffer vkCreateBuffer = nullptr;
PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
PFN_vkAllocateMemory vkAllocateMemory = nullptr;
PFN_vkFreeMemory vkFreeMemory = nullptr;
PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
PFN_vkMapMemory vkMapMemory = nullptr;
PFN_vkUnmapMemory vkUnmapMemory = nullptr;
PFN_vkCreateImage vkCreateImage = nullptr;
PFN_vkDestroyImage vkDestroyImage = nullptr;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
PFN_vkBindImageMemory vkBindImageMemory = nullptr;
PFN_vkCreateSampler vkCreateSampler = nullptr;
PFN_vkDestroySampler vkDestroySampler = nullptr;
PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
PFN_vkCmdDraw vkCmdDraw = nullptr;
PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
PFN_vkCreateFence vkCreateFence = nullptr;
PFN_vkDestroyFence vkDestroyFence = nullptr;
PFN_vkWaitForFences vkWaitForFences = nullptr;
PFN_vkResetFences vkResetFences = nullptr;
PFN_vkQueueSubmit vkQueueSubmit = nullptr;
PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;

HMODULE vulkanLibraryHandle() {
    static HMODULE library = nullptr;
    if (!library) {
        library = LoadLibraryW(L"vulkan-1.dll");
        if (!library) {
            throw std::runtime_error("Could not load vulkan-1.dll");
        }
    }
    return library;
}

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CameraUniform {
    float eyeNear[4]{};
    float rightFar[4]{};
    float upTanHalf[4]{};
    float forwardAspect[4]{};
};

struct QueueFamilies {
    std::uint32_t graphics = 0;
    std::uint32_t present = 0;
    bool hasGraphics = false;
    bool hasPresent = false;
};

struct TextureResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 0.00001f) {
        return {};
    }
    return v * (1.0f / len);
}

Vec3 eyeOf(const V1CameraSettings& camera) {
    return {camera.eyeX, camera.eyeY, camera.eyeZ};
}

Vec3 targetOf(const V1CameraSettings& camera) {
    return {camera.targetX, camera.targetY, camera.targetZ};
}

Vec3 forwardFor(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return normalize({cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch)});
}

Vec3 rightFor(Vec3 forward) {
    const Vec3 right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    return dot(right, right) <= 0.00001f ? Vec3{1.0f, 0.0f, 0.0f} : right;
}

std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open SPIR-V shader: " + path.string());
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("Invalid SPIR-V shader size: " + path.string());
    }
    std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

std::filesystem::path executablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer);
}

std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) {
    if (relativePath.is_absolute() || std::filesystem::exists(relativePath)) {
        return relativePath;
    }

    std::filesystem::path cursor = executablePath().parent_path();
    while (!cursor.empty()) {
        const std::filesystem::path candidate = cursor / relativePath;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return relativePath;
}

std::filesystem::path projectRootPath() {
    std::filesystem::path cursor = executablePath().parent_path();
    while (!cursor.empty()) {
        if (std::filesystem::exists(cursor / "assets") && std::filesystem::exists(cursor / "shaders")) {
            return cursor;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return std::filesystem::current_path();
}

std::filesystem::path previewLogPath() {
    const std::filesystem::path path = projectRootPath() / "out/vulkan-preview.log";
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    return path;
}

void previewLog(const std::string& message) {
    std::ofstream out(previewLogPath(), std::ios::app);
    out << message << '\n';
}

void resetPreviewLog() {
    std::ofstream out(previewLogPath(), std::ios::trunc);
    out << "Vulkan preview log\n";
}

void require(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed");
    }
}

template <class T>
void loadGlobal(HMODULE library, T& target, const char* name) {
    target = reinterpret_cast<T>(GetProcAddress(library, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan global function: ") + name);
    }
}

template <class T>
void loadInstance(VkInstance instance, T& target, const char* name) {
    target = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan instance function: ") + name);
    }
}

template <class T>
void loadDevice(VkDevice device, T& target, const char* name) {
    target = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan device function: ") + name);
    }
}

void loadVulkanLibrary() {
    static bool loaded = false;
    if (loaded) {
        return;
    }
    SetEnvironmentVariableW(
        L"VK_LOADER_LAYERS_DISABLE",
        L"VK_LAYER_VALVE_steam_overlay,VK_LAYER_VALVE_steam_fossilize"
    );
    SetEnvironmentVariableW(L"DISABLE_LAYER_NV_OPTIMUS_1", L"1");
    SetEnvironmentVariableW(L"DISABLE_LAYER_NV_PRESENT_1", L"1");
    HMODULE library = vulkanLibraryHandle();
    loadGlobal(library, vkGetInstanceProcAddr, "vkGetInstanceProcAddr");
    loadGlobal(library, vkCreateInstance, "vkCreateInstance");
    loaded = true;
}

std::uint32_t findMemoryType(VkPhysicalDevice physicalDevice, std::uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    throw std::runtime_error("No compatible Vulkan memory type found");
}

class VulkanGpuRenderer {
public:
    VulkanGpuRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height, const GpuPreviewGeometry& geometry)
        : hwnd_(hwnd), width_(width), height_(height), geometry_(geometry) {
        previewLog("VulkanGpuRenderer: loadVulkanLibrary");
        loadVulkanLibrary();
        previewLog("VulkanGpuRenderer: createInstance");
        createInstance();
        previewLog("VulkanGpuRenderer: loadInstanceFunctions");
        loadInstanceFunctions();
        previewLog("VulkanGpuRenderer: createSurface");
        createSurface();
        previewLog("VulkanGpuRenderer: selectDevice");
        selectDevice();
        previewLog("VulkanGpuRenderer: createDevice");
        createDevice();
        previewLog("VulkanGpuRenderer: loadDeviceFunctions");
        loadDeviceFunctions();
        previewLog("VulkanGpuRenderer: fetchDeviceQueues");
        fetchDeviceQueues();
        previewLog("VulkanGpuRenderer: createSwapchain");
        createSwapchain();
        previewLog("VulkanGpuRenderer: createRenderPass");
        createRenderPass();
        previewLog("VulkanGpuRenderer: createDepthResources");
        createDepthResources();
        previewLog("VulkanGpuRenderer: createFramebuffers");
        createFramebuffers();
        previewLog("VulkanGpuRenderer: createBuffers");
        createBuffers();
        previewLog("VulkanGpuRenderer: createTextureResources");
        createTextureResources();
        previewLog("VulkanGpuRenderer: createDescriptors");
        createDescriptors();
        previewLog("VulkanGpuRenderer: createPipeline");
        createPipeline();
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
        if (device_ != VK_NULL_HANDLE && skyPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, skyPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && textureSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, textureSampler_, nullptr);
        for (TextureResource& texture : textures_) {
            if (device_ != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && uniformBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, uniformBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && uniformMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, uniformMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexMemory_, nullptr);
        for (VkFramebuffer framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
        if (device_ != VK_NULL_HANDLE && depthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, depthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, depthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, depthMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
        for (VkImageView view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
        if (device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    void draw(const V1CameraSettings& camera) {
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
        updateUniform(camera);
        if (logFrame) previewLog("draw: record command buffer");
        recordCommandBuffer(commandBuffer_, imageIndex);

        if (logFrame) previewLog("draw: submit");
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
        appInfo.apiVersion = VK_API_VERSION_1_0;

        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;
        require(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
    }

    void loadInstanceFunctions() {
        loadInstance(instance_, vkDestroyInstance, "vkDestroyInstance");
        loadInstance(instance_, vkCreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR");
        loadInstance(instance_, vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
        loadInstance(instance_, vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
        loadInstance(instance_, vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
        loadInstance(instance_, vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        loadInstance(instance_, vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
        loadInstance(instance_, vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
        loadInstance(instance_, vkCreateDevice, "vkCreateDevice");
        loadInstance(instance_, vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
    }

    void loadDeviceFunctions() {
        loadDevice(device_, vkDestroyDevice, "vkDestroyDevice");
        loadDevice(device_, vkGetDeviceQueue, "vkGetDeviceQueue");
        loadDevice(device_, vkDeviceWaitIdle, "vkDeviceWaitIdle");
        loadDevice(device_, vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
        loadDevice(device_, vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
        loadDevice(device_, vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
        loadDevice(device_, vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
        loadDevice(device_, vkQueuePresentKHR, "vkQueuePresentKHR");
        loadDevice(device_, vkCreateImageView, "vkCreateImageView");
        loadDevice(device_, vkDestroyImageView, "vkDestroyImageView");
        loadDevice(device_, vkCreateRenderPass, "vkCreateRenderPass");
        loadDevice(device_, vkDestroyRenderPass, "vkDestroyRenderPass");
        loadDevice(device_, vkCreateFramebuffer, "vkCreateFramebuffer");
        loadDevice(device_, vkDestroyFramebuffer, "vkDestroyFramebuffer");
        loadDevice(device_, vkCreateShaderModule, "vkCreateShaderModule");
        loadDevice(device_, vkDestroyShaderModule, "vkDestroyShaderModule");
        loadDevice(device_, vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
        loadDevice(device_, vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
        loadDevice(device_, vkCreateDescriptorPool, "vkCreateDescriptorPool");
        loadDevice(device_, vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
        loadDevice(device_, vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
        loadDevice(device_, vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
        loadDevice(device_, vkCreatePipelineLayout, "vkCreatePipelineLayout");
        loadDevice(device_, vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
        loadDevice(device_, vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
        loadDevice(device_, vkDestroyPipeline, "vkDestroyPipeline");
        loadDevice(device_, vkCreateBuffer, "vkCreateBuffer");
        loadDevice(device_, vkDestroyBuffer, "vkDestroyBuffer");
        loadDevice(device_, vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
        loadDevice(device_, vkAllocateMemory, "vkAllocateMemory");
        loadDevice(device_, vkFreeMemory, "vkFreeMemory");
        loadDevice(device_, vkBindBufferMemory, "vkBindBufferMemory");
        loadDevice(device_, vkMapMemory, "vkMapMemory");
        loadDevice(device_, vkUnmapMemory, "vkUnmapMemory");
        loadDevice(device_, vkCreateImage, "vkCreateImage");
        loadDevice(device_, vkDestroyImage, "vkDestroyImage");
        loadDevice(device_, vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
        loadDevice(device_, vkBindImageMemory, "vkBindImageMemory");
        loadDevice(device_, vkCreateSampler, "vkCreateSampler");
        loadDevice(device_, vkDestroySampler, "vkDestroySampler");
        loadDevice(device_, vkCreateCommandPool, "vkCreateCommandPool");
        loadDevice(device_, vkDestroyCommandPool, "vkDestroyCommandPool");
        loadDevice(device_, vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
        loadDevice(device_, vkResetCommandBuffer, "vkResetCommandBuffer");
        loadDevice(device_, vkBeginCommandBuffer, "vkBeginCommandBuffer");
        loadDevice(device_, vkEndCommandBuffer, "vkEndCommandBuffer");
        loadDevice(device_, vkFreeCommandBuffers, "vkFreeCommandBuffers");
        loadDevice(device_, vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
        loadDevice(device_, vkCmdEndRenderPass, "vkCmdEndRenderPass");
        loadDevice(device_, vkCmdBindPipeline, "vkCmdBindPipeline");
        loadDevice(device_, vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
        loadDevice(device_, vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
        loadDevice(device_, vkCmdSetViewport, "vkCmdSetViewport");
        loadDevice(device_, vkCmdSetScissor, "vkCmdSetScissor");
        loadDevice(device_, vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
        loadDevice(device_, vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
        loadDevice(device_, vkCmdDraw, "vkCmdDraw");
        loadDevice(device_, vkCreateSemaphore, "vkCreateSemaphore");
        loadDevice(device_, vkDestroySemaphore, "vkDestroySemaphore");
        loadDevice(device_, vkCreateFence, "vkCreateFence");
        loadDevice(device_, vkDestroyFence, "vkDestroyFence");
        loadDevice(device_, vkWaitForFences, "vkWaitForFences");
        loadDevice(device_, vkResetFences, "vkResetFences");
        loadDevice(device_, vkQueueSubmit, "vkQueueSubmit");
        loadDevice(device_, vkQueueWaitIdle, "vkQueueWaitIdle");
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
            const bool hasSwapchain = std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties& ext) {
                return std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });
            if (!hasSwapchain) {
                continue;
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
            gpuName_ = properties.deviceName;
            previewLog(
                "selectDevice: " + gpuName_
                + " graphicsQueue=" + std::to_string(queueFamilies_.graphics)
                + " presentQueue=" + std::to_string(queueFamilies_.present)
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

        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = extensions;
        require(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");
    }

    void fetchDeviceQueues() {
        vkGetDeviceQueue(device_, queueFamilies_.graphics, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, queueFamilies_.present, 0, &presentQueue_);
    }

    void createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

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
        swapchainImageViews_.reserve(swapchainImages_.size());
        for (VkImage image : swapchainImages_) {
            swapchainImageViews_.push_back(createImageView(image, swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT));
        }
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        require(vkCreateImageView(device_, &createInfo, nullptr, &view), "vkCreateImageView");
        return view;
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapchainFormat_;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkAttachmentDescription, 2> attachments{color, depth};
        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        require(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_), "vkCreateRenderPass");
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
        image.samples = VK_SAMPLE_COUNT_1_BIT;
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

    void createFramebuffers() {
        framebuffers_.reserve(swapchainImageViews_.size());
        for (VkImageView view : swapchainImageViews_) {
            std::array<VkImageView, 2> attachments{view, depthView_};
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass_;
            createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            createInfo.width = swapchainExtent_.width;
            createInfo.height = swapchainExtent_.height;
            createInfo.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            require(vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            framebuffers_.push_back(framebuffer);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
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
        allocate.memoryTypeIndex = findMemoryType(
            physicalDevice_,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        require(vkAllocateMemory(device_, &allocate, nullptr, &memory), "vkAllocateMemory(buffer)");
        require(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
    }

    void createBuffers() {
        vertexCount_ = static_cast<std::uint32_t>(geometry_.vertices.size());
        vertexBytes_ = std::max<VkDeviceSize>(1, sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        createBuffer(vertexBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexMemory_);
        void* mapped = nullptr;
        require(vkMapMemory(device_, vertexMemory_, 0, vertexBytes_, 0, &mapped), "vkMapMemory(vertex)");
        std::memcpy(mapped, geometry_.vertices.data(), sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        vkUnmapMemory(device_, vertexMemory_);

        createBuffer(sizeof(CameraUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffer_, uniformMemory_);
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

    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
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
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureResources() {
        createSampledTexture(geometry_.albedoTexturePath, {255, 255, 255, 255}, textures_[0]);
        createSampledTexture(geometry_.normalTexturePath, {128, 128, 255, 255}, textures_[1]);
        createSampledTexture(geometry_.roughnessTexturePath, {178, 178, 178, 255}, textures_[2]);
        createSampledTexture(geometry_.displacementTexturePath, {128, 128, 128, 255}, textures_[3]);

        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.maxLod = 0.0f;
        require(vkCreateSampler(device_, &sampler, nullptr, &textureSampler_), "vkCreateSampler(texture)");
    }

    void createDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
        bindings[0] = cameraBinding;
        for (std::uint32_t i = 0; i < 4; ++i) {
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
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &descriptorSetLayout_), "vkCreateDescriptorSetLayout");

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[1].descriptorCount = 4;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[2].descriptorCount = 1;
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 1;
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptorPool_), "vkCreateDescriptorPool");

        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = descriptorPool_;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &descriptorSetLayout_;
        require(vkAllocateDescriptorSets(device_, &allocate, &descriptorSet_), "vkAllocateDescriptorSets");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;

        std::array<VkWriteDescriptorSet, 6> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSet_;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &bufferInfo;
        std::array<VkDescriptorImageInfo, 4> imageInfos{};
        for (std::uint32_t i = 0; i < 4; ++i) {
            imageInfos[i].imageView = textures_[i].view;
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writes[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1 + i].dstSet = descriptorSet_;
            writes[1 + i].dstBinding = 1 + i;
            writes[1 + i].descriptorCount = 1;
            writes[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[1 + i].pImageInfo = &imageInfos[i];
        }
        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = descriptorSet_;
        writes[5].dstBinding = 5;
        writes[5].descriptorCount = 1;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[5].pImageInfo = &samplerInfo;
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    VkShaderModule createShader(const std::filesystem::path& path) {
        const std::vector<std::uint32_t> code = readSpirv(resolveProjectPath(path));
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(std::uint32_t);
        createInfo.pCode = code.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        require(vkCreateShaderModule(device_, &createInfo, nullptr, &shader), "vkCreateShaderModule");
        return shader;
    }

    void createPipeline() {
        const VkShaderModule vertexShader = createShader("shaders/vulkan_gpu/simple_color.vert.spv");
        const VkShaderModule fragmentShader = createShader("shaders/vulkan_gpu/simple_color.frag.spv");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentShader;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(GpuPreviewVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        std::array<VkVertexInputAttributeDescription, 8> attributes{};
        attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, px)};
        attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, nx)};
        attributes[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, r)};
        attributes[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GpuPreviewVertex, u)};
        attributes[4] = {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, textured)};
        attributes[5] = {5, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, roughness)};
        attributes[6] = {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, metalness)};
        attributes[7] = {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, kind)};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlend{};
        colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &colorBlend;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(std::size(dynamicStates));
        dynamic.pDynamicStates = dynamicStates;

        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptorSetLayout_;
        require(vkCreatePipelineLayout(device_, &layout, nullptr, &pipelineLayout_), "vkCreatePipelineLayout");

        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.stageCount = 2;
        createInfo.pStages = stages;
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pViewportState = &viewportState;
        createInfo.pRasterizationState = &raster;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depth;
        createInfo.pColorBlendState = &blend;
        createInfo.pDynamicState = &dynamic;
        createInfo.layout = pipelineLayout_;
        createInfo.renderPass = renderPass_;
        createInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline_), "vkCreateGraphicsPipelines");

        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);

        const VkShaderModule skyVertexShader = createShader("shaders/vulkan_gpu/skybox.vert.spv");
        const VkShaderModule skyFragmentShader = createShader("shaders/vulkan_gpu/skybox.frag.spv");
        VkPipelineShaderStageCreateInfo skyStages[2]{};
        skyStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        skyStages[0].module = skyVertexShader;
        skyStages[0].pName = "main";
        skyStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        skyStages[1].module = skyFragmentShader;
        skyStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo skyVertexInput{};
        skyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineRasterizationStateCreateInfo skyRaster{};
        skyRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        skyRaster.polygonMode = VK_POLYGON_MODE_FILL;
        skyRaster.cullMode = VK_CULL_MODE_NONE;
        skyRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        skyRaster.lineWidth = 1.0f;
        VkPipelineDepthStencilStateCreateInfo skyDepth{};
        skyDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        skyDepth.depthTestEnable = VK_FALSE;
        skyDepth.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo skyCreateInfo{};
        skyCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        skyCreateInfo.stageCount = 2;
        skyCreateInfo.pStages = skyStages;
        skyCreateInfo.pVertexInputState = &skyVertexInput;
        skyCreateInfo.pInputAssemblyState = &inputAssembly;
        skyCreateInfo.pViewportState = &viewportState;
        skyCreateInfo.pRasterizationState = &skyRaster;
        skyCreateInfo.pMultisampleState = &multisample;
        skyCreateInfo.pDepthStencilState = &skyDepth;
        skyCreateInfo.pColorBlendState = &blend;
        skyCreateInfo.pDynamicState = &dynamic;
        skyCreateInfo.layout = pipelineLayout_;
        skyCreateInfo.renderPass = renderPass_;
        skyCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &skyCreateInfo, nullptr, &skyPipeline_), "vkCreateGraphicsPipelines(sky)");

        vkDestroyShaderModule(device_, skyFragmentShader, nullptr);
        vkDestroyShaderModule(device_, skyVertexShader, nullptr);
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

    void updateUniform(const V1CameraSettings& cameraSettings) {
        const Vec3 eye = eyeOf(cameraSettings);
        const Vec3 forward = normalize(targetOf(cameraSettings) - eye);
        const Vec3 right = rightFor(forward);
        const Vec3 up = normalize(cross(right, forward));
        const float nearPlane = cameraSettings.nearPlane > 0.0f ? cameraSettings.nearPlane : 0.05f;
        const float farPlane = cameraSettings.farPlane > 0.0f ? cameraSettings.farPlane : 200.0f;
        const float fovY = cameraSettings.fovY > 0.0f ? cameraSettings.fovY : (52.0f * kPi / 180.0f);
        const float aspect = static_cast<float>(swapchainExtent_.width) / static_cast<float>(swapchainExtent_.height);

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

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent = swapchainExtent_;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &offset);
        vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
        require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
    }

    HWND hwnd_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    const GpuPreviewGeometry& geometry_;
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
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexBytes_ = 0;
    std::uint32_t vertexCount_ = 0;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory_ = VK_NULL_HANDLE;
    std::array<TextureResource, 4> textures_{};
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline skyPipeline_ = VK_NULL_HANDLE;
    VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;
    std::uint32_t frameIndex_ = 0;
};

struct PreviewState {
    V1RenderSettings settings;
    GpuPreviewGeometry geometry;
    std::unique_ptr<VulkanGpuRenderer> renderer;
    std::array<bool, 256> keys{};
    V1CameraSettings camera;
    bool roaming = false;
    float yaw = 0.0f;
    float pitch = 0.0f;
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
    const Vec3 right = rightFor(forward);
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

void updateCamera(PreviewState& state, float dt) {
    if (!state.roaming) {
        state.camera = state.geometry.camera;
        return;
    }
    const float lookSpeed = 1.85f;
    if (keyDown(state, VK_LEFT) || keyDown(state, 'J')) state.yaw += lookSpeed * dt;
    if (keyDown(state, VK_RIGHT) || keyDown(state, 'L')) state.yaw -= lookSpeed * dt;
    if (keyDown(state, VK_UP) || keyDown(state, 'I')) state.pitch = std::clamp(state.pitch + lookSpeed * dt, -1.45f, 1.45f);
    if (keyDown(state, VK_DOWN) || keyDown(state, 'K')) state.pitch = std::clamp(state.pitch - lookSpeed * dt, -1.45f, 1.45f);

    const Vec3 forward = forwardFor(state.yaw, state.pitch);
    const Vec3 right = rightFor(forward);
    const Vec3 up{0.0f, 0.0f, 1.0f};
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
                state->renderer->draw(state->camera);
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
                        std::cout << "GPU camera roaming enabled. WASD move, Q/E vertical, arrows/IJKL look, Shift fast.\n";
                    } else {
                        state->camera = state->geometry.camera;
                        SetWindowTextW(hwnd, L"VulkanRender GPU preview - press R for roaming");
                        std::cout << "GPU camera roaming disabled.\n";
                    }
                }
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
        L"VulkanRender GPU preview - press R for roaming",
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
        state.renderer = std::make_unique<VulkanGpuRenderer>(hwnd, state.settings.width, state.settings.height, state.geometry);
    } catch (const std::exception& error) {
        std::cerr << "Could not initialize Vulkan GPU preview: " << error.what() << '\n';
        previewLog(std::string("runVulkanPreviewWindow: renderer init failed: ") + error.what());
        DestroyWindow(hwnd);
        return 1;
    }
    previewLog("runVulkanPreviewWindow: timer start");
    SetTimer(hwnd, kWindowTimer, 16, nullptr);

    std::cout << "Opened Vulkan GPU preview window. vertices=" << state.geometry.vertices.size()
              << ". Press R for roaming, Esc to exit.\n";

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    state.renderer.reset();
    return 0;
}

} // namespace vr
