#include "rhi/VulkanDevice.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if VR_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace vr {
namespace {

const std::vector<std::string> kRayTracingExtensions = {
    "VK_KHR_acceleration_structure",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_deferred_host_operations",
    "VK_KHR_buffer_device_address",
};

#if VR_HAS_VULKAN

std::string versionString(std::uint32_t version) {
    std::ostringstream out;
    out << VK_VERSION_MAJOR(version) << '.'
        << VK_VERSION_MINOR(version) << '.'
        << VK_VERSION_PATCH(version);
    return out.str();
}

std::string deviceTypeString(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
    }
}

class VulkanRenderDevice final : public RenderDevice {
public:
    explicit VulkanRenderDevice(bool validation) {
        report_.backendName = "vulkan";
        report_.validationRequested = validation;
        enumerate();
    }

    ~VulkanRenderDevice() override {
        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
        }
    }

    DeviceReport report() const override {
        return report_;
    }

private:
    void enumerate() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "LZJU VulkanRender";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "LZJU Rewrite";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        const VkResult createResult = vkCreateInstance(&createInfo, nullptr, &instance_);
        if (createResult != VK_SUCCESS) {
            report_.available = false;
            report_.messages.push_back("vkCreateInstance failed; check Vulkan runtime, driver, or loader installation.");
            return;
        }

        report_.available = true;

        std::uint32_t deviceCount = 0;
        VkResult result = vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        if (result != VK_SUCCESS || deviceCount == 0) {
            report_.messages.push_back("No Vulkan physical devices were reported by the loader.");
            return;
        }

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        result = vkEnumeratePhysicalDevices(instance_, &deviceCount, physicalDevices.data());
        if (result != VK_SUCCESS) {
            report_.messages.push_back("vkEnumeratePhysicalDevices failed while fetching device handles.");
            return;
        }

        for (const VkPhysicalDevice physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);

            DeviceInfo info;
            info.name = properties.deviceName;
            info.apiVersion = versionString(properties.apiVersion);
            info.type = deviceTypeString(properties.deviceType);

            std::uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            if (extensionCount > 0) {
                vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
            }

            std::set<std::string> availableExtensions;
            for (const auto& extension : extensions) {
                availableExtensions.insert(extension.extensionName);
            }

            for (const auto& required : kRayTracingExtensions) {
                if (!availableExtensions.contains(required)) {
                    info.missingRayTracingExtensions.push_back(required);
                }
            }
            info.supportsRayTracing = info.missingRayTracingExtensions.empty();
            report_.devices.push_back(std::move(info));
        }
    }

    VkInstance instance_ = VK_NULL_HANDLE;
    DeviceReport report_;
};

#endif

} // namespace

std::unique_ptr<RenderDevice> createVulkanDevice(bool enableValidation) {
#if VR_HAS_VULKAN
    return std::make_unique<VulkanRenderDevice>(enableValidation);
#else
    (void)enableValidation;
    return nullptr;
#endif
}

} // namespace vr

