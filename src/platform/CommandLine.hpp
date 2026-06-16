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
};

class CommandLine {
public:
    static AppConfig parse(int argc, char** argv);
    static void printHelp(std::ostream& out);
};

} // namespace vr
