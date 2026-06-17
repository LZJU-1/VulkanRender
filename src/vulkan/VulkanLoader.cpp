#include "vulkan/VulkanLoader.hpp"

namespace vr {

// --- Vulkan global function pointer definitions ---
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkCreateInstance vkCreateInstance = nullptr;
PFN_vkDestroyInstance vkDestroyInstance = nullptr;
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2 = nullptr;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties = nullptr;
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
PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
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
PFN_vkCmdDispatch vkCmdDispatch = nullptr;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
PFN_vkCmdBlitImage vkCmdBlitImage = nullptr;
PFN_vkCmdDraw vkCmdDraw = nullptr;
PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
PFN_vkCreateFence vkCreateFence = nullptr;
PFN_vkDestroyFence vkDestroyFence = nullptr;
PFN_vkWaitForFences vkWaitForFences = nullptr;
PFN_vkResetFences vkResetFences = nullptr;
PFN_vkQueueSubmit vkQueueSubmit = nullptr;
PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

namespace {

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

template <class T>
void loadGlobal(HMODULE library, T& target, const char* name) {  // kept for internal use, prefer loadGlobalFunc from header
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

} // anonymous namespace

void require(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed");
    }
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

void loadInstanceFunctions(VkInstance instance) {
    loadInstance(instance, vkDestroyInstance, "vkDestroyInstance");
    loadInstance(instance, vkCreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR");
    loadInstance(instance, vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
    loadInstance(instance, vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
    loadInstance(instance, vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
    loadInstance(instance, vkGetPhysicalDeviceFeatures, "vkGetPhysicalDeviceFeatures");
    loadInstance(instance, vkGetPhysicalDeviceFeatures2, "vkGetPhysicalDeviceFeatures2");
    loadInstance(instance, vkGetPhysicalDeviceFormatProperties, "vkGetPhysicalDeviceFormatProperties");
    loadInstance(instance, vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
    loadInstance(instance, vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
    loadInstance(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    loadInstance(instance, vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    loadInstance(instance, vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    loadInstance(instance, vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
    loadInstance(instance, vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
    loadInstance(instance, vkCreateDevice, "vkCreateDevice");
    loadInstance(instance, vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
}

void loadDeviceFunctions(VkDevice device, bool enableRayTracing) {
    loadDevice(device, vkDestroyDevice, "vkDestroyDevice");
    loadDevice(device, vkGetDeviceQueue, "vkGetDeviceQueue");
    loadDevice(device, vkDeviceWaitIdle, "vkDeviceWaitIdle");
    loadDevice(device, vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
    loadDevice(device, vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
    loadDevice(device, vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
    loadDevice(device, vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
    loadDevice(device, vkQueuePresentKHR, "vkQueuePresentKHR");
    loadDevice(device, vkCreateImageView, "vkCreateImageView");
    loadDevice(device, vkDestroyImageView, "vkDestroyImageView");
    loadDevice(device, vkCreateRenderPass, "vkCreateRenderPass");
    loadDevice(device, vkDestroyRenderPass, "vkDestroyRenderPass");
    loadDevice(device, vkCreateFramebuffer, "vkCreateFramebuffer");
    loadDevice(device, vkDestroyFramebuffer, "vkDestroyFramebuffer");
    loadDevice(device, vkCreateShaderModule, "vkCreateShaderModule");
    loadDevice(device, vkDestroyShaderModule, "vkDestroyShaderModule");
    loadDevice(device, vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    loadDevice(device, vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    loadDevice(device, vkCreateDescriptorPool, "vkCreateDescriptorPool");
    loadDevice(device, vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
    loadDevice(device, vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    loadDevice(device, vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    loadDevice(device, vkCreatePipelineLayout, "vkCreatePipelineLayout");
    loadDevice(device, vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
    loadDevice(device, vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
    loadDevice(device, vkCreateComputePipelines, "vkCreateComputePipelines");
    loadDevice(device, vkDestroyPipeline, "vkDestroyPipeline");
    loadDevice(device, vkCreateBuffer, "vkCreateBuffer");
    loadDevice(device, vkDestroyBuffer, "vkDestroyBuffer");
    loadDevice(device, vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
    loadDevice(device, vkAllocateMemory, "vkAllocateMemory");
    loadDevice(device, vkFreeMemory, "vkFreeMemory");
    loadDevice(device, vkBindBufferMemory, "vkBindBufferMemory");
    loadDevice(device, vkMapMemory, "vkMapMemory");
    loadDevice(device, vkUnmapMemory, "vkUnmapMemory");
    loadDevice(device, vkCreateImage, "vkCreateImage");
    loadDevice(device, vkDestroyImage, "vkDestroyImage");
    loadDevice(device, vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
    loadDevice(device, vkBindImageMemory, "vkBindImageMemory");
    loadDevice(device, vkCreateSampler, "vkCreateSampler");
    loadDevice(device, vkDestroySampler, "vkDestroySampler");
    loadDevice(device, vkCreateCommandPool, "vkCreateCommandPool");
    loadDevice(device, vkDestroyCommandPool, "vkDestroyCommandPool");
    loadDevice(device, vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    loadDevice(device, vkResetCommandBuffer, "vkResetCommandBuffer");
    loadDevice(device, vkBeginCommandBuffer, "vkBeginCommandBuffer");
    loadDevice(device, vkEndCommandBuffer, "vkEndCommandBuffer");
    loadDevice(device, vkFreeCommandBuffers, "vkFreeCommandBuffers");
    loadDevice(device, vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
    loadDevice(device, vkCmdEndRenderPass, "vkCmdEndRenderPass");
    loadDevice(device, vkCmdBindPipeline, "vkCmdBindPipeline");
    loadDevice(device, vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    loadDevice(device, vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
    loadDevice(device, vkCmdSetViewport, "vkCmdSetViewport");
    loadDevice(device, vkCmdSetScissor, "vkCmdSetScissor");
    loadDevice(device, vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
    loadDevice(device, vkCmdDispatch, "vkCmdDispatch");
    loadDevice(device, vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
    loadDevice(device, vkCmdBlitImage, "vkCmdBlitImage");
    loadDevice(device, vkCmdDraw, "vkCmdDraw");
    loadDevice(device, vkCreateSemaphore, "vkCreateSemaphore");
    loadDevice(device, vkDestroySemaphore, "vkDestroySemaphore");
    loadDevice(device, vkCreateFence, "vkCreateFence");
    loadDevice(device, vkDestroyFence, "vkDestroyFence");
    loadDevice(device, vkWaitForFences, "vkWaitForFences");
    loadDevice(device, vkResetFences, "vkResetFences");
    loadDevice(device, vkQueueSubmit, "vkQueueSubmit");
    loadDevice(device, vkQueueWaitIdle, "vkQueueWaitIdle");
    if (enableRayTracing) {
        loadDevice(device, vkGetBufferDeviceAddressKHR, "vkGetBufferDeviceAddressKHR");
        loadDevice(device, vkCreateAccelerationStructureKHR, "vkCreateAccelerationStructureKHR");
        loadDevice(device, vkDestroyAccelerationStructureKHR, "vkDestroyAccelerationStructureKHR");
        loadDevice(device, vkGetAccelerationStructureBuildSizesKHR, "vkGetAccelerationStructureBuildSizesKHR");
        loadDevice(device, vkCmdBuildAccelerationStructuresKHR, "vkCmdBuildAccelerationStructuresKHR");
        loadDevice(device, vkGetAccelerationStructureDeviceAddressKHR, "vkGetAccelerationStructureDeviceAddressKHR");
        loadDevice(device, vkCreateRayTracingPipelinesKHR, "vkCreateRayTracingPipelinesKHR");
        loadDevice(device, vkCmdTraceRaysKHR, "vkCmdTraceRaysKHR");
        loadDevice(device, vkGetRayTracingShaderGroupHandlesKHR, "vkGetRayTracingShaderGroupHandlesKHR");
    }
}

} // namespace vr
