#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace vr {

struct DeviceInfo {
    std::string name;
    std::string apiVersion;
    std::string type;
    bool supportsRayTracing = false;
    std::vector<std::string> missingRayTracingExtensions;
};

struct DeviceReport {
    std::string backendName;
    bool available = false;
    bool validationRequested = false;
    std::vector<DeviceInfo> devices;
    std::vector<std::string> messages;

    bool hasRayTracingDevice() const;
    void print(std::ostream& out) const;
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    virtual DeviceReport report() const = 0;
};

std::unique_ptr<RenderDevice> createRenderDevice(bool preferVulkan, bool enableValidation);

} // namespace vr

