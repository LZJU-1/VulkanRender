#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace vr {

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
};

class CommandLine {
public:
    static AppConfig parse(int argc, char** argv);
    static void printHelp(std::ostream& out);
};

} // namespace vr
