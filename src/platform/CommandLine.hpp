#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace vr {

struct AppConfig {
    std::string profile = "v1";
    std::string scenePath;
    std::uint32_t frames = 1;
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

