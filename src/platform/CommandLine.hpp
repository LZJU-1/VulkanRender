#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace vr {

struct CameraOverride {
    bool enabled = false;
    float eyeX = 0.0f;
    float eyeY = 0.0f;
    float eyeZ = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float fovYDegrees = 0.0f;
};

struct AppConfig {
    std::string profile = "v1";
    std::string scenePath;
    std::string outputPath;
    std::uint32_t frames = 1;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool renderImage = false;
    bool previewWindow = false;
    bool listDevices = false;
    bool enableValidation = false;
    bool enableRayTracing = false;
    bool requireRayTracing = false;
    bool headless = false;
    bool printValidationPipeline = false;
    std::string validationPipelineProfile;
    CameraOverride camera;
    std::uint32_t v5QualityLevel = 1;  // 0=low(8spp), 1=medium(16spp), 2=high(32spp), 3=ultra(64spp)
    static std::uint32_t v5QualitySampleCount(std::uint32_t level) {
        return 8u << std::min<std::uint32_t>(level, 3u);  // 8, 16, 32, 64
    }
};

class CommandLine {
public:
    static AppConfig parse(int argc, char** argv);
    static void printHelp(std::ostream& out);
};

} // namespace vr
