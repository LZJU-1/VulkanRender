#include "rhi/RenderDevice.hpp"

#include "rhi/VulkanDevice.hpp"

#include <ostream>
#include <utility>

namespace vr {
namespace {

class MockRenderDevice final : public RenderDevice {
public:
    explicit MockRenderDevice(bool validation) : validation_(validation) {}

    DeviceReport report() const override {
        DeviceReport report;
        report.backendName = "mock";
        report.available = true;
        report.validationRequested = validation_;
        report.messages.push_back("Vulkan SDK was not available at configure time; using mock backend.");
        return report;
    }

private:
    bool validation_ = false;
};

} // namespace

bool DeviceReport::hasRayTracingDevice() const {
    for (const auto& device : devices) {
        if (device.supportsRayTracing) {
            return true;
        }
    }
    return false;
}

void DeviceReport::print(std::ostream& out) const {
    out << "Backend: " << backendName << '\n';
    out << "Available: " << (available ? "yes" : "no") << '\n';
    out << "Validation requested: " << (validationRequested ? "yes" : "no") << '\n';

    if (devices.empty()) {
        out << "Devices: none\n";
    } else {
        out << "Devices:\n";
        for (const auto& device : devices) {
            out << "  - " << device.name << " (" << device.type << ", Vulkan " << device.apiVersion << ")\n";
            out << "    Ray tracing: " << (device.supportsRayTracing ? "supported" : "not supported") << '\n';
            if (!device.missingRayTracingExtensions.empty()) {
                out << "    Missing:";
                for (const auto& extension : device.missingRayTracingExtensions) {
                    out << ' ' << extension;
                }
                out << '\n';
            }
        }
    }

    for (const auto& message : messages) {
        out << "Note: " << message << '\n';
    }
}

std::unique_ptr<RenderDevice> createRenderDevice(bool preferVulkan, bool enableValidation) {
    if (preferVulkan) {
        auto vulkan = createVulkanDevice(enableValidation);
        if (vulkan) {
            return vulkan;
        }
    }
    return std::make_unique<MockRenderDevice>(enableValidation);
}

} // namespace vr

