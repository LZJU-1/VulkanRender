#include "platform/CommandLine.hpp"

#include "core/FeatureProfile.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace vr {
namespace {

std::uint32_t parseU32(std::string_view value, std::string_view option) {
    std::uint32_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("Invalid integer for " + std::string(option) + ": " + std::string(value));
    }
    return parsed;
}

std::string_view requireValue(int& index, int argc, char** argv, std::string_view option) {
    if (index + 1 >= argc) {
        throw std::runtime_error("Missing value after " + std::string(option));
    }
    ++index;
    return argv[index];
}

} // namespace

AppConfig CommandLine::parse(int argc, char** argv) {
    AppConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp(std::cout);
            std::exit(0);
        }
        if (arg == "--profile") {
            config.profile = std::string(requireValue(i, argc, argv, arg));
        } else if (arg == "--scene") {
            config.scenePath = std::string(requireValue(i, argc, argv, arg));
        } else if (arg == "--output") {
            config.outputPath = std::string(requireValue(i, argc, argv, arg));
            config.renderImage = true;
        } else if (arg == "--width") {
            config.width = parseU32(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--height") {
            config.height = parseU32(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--render") {
            config.renderImage = true;
        } else if (arg == "--preview") {
            config.previewWindow = true;
        } else if (arg == "--frames") {
            config.frames = parseU32(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--list-devices") {
            config.listDevices = true;
        } else if (arg == "--validate") {
            config.enableValidation = true;
        } else if (arg == "--enable-rt") {
            config.enableRayTracing = true;
            config.profile = "v5-rt";
        } else if (arg == "--require-rt") {
            config.enableRayTracing = true;
            config.requireRayTracing = true;
            config.profile = "v5-rt";
        } else if (arg == "--headless") {
            config.headless = true;
        } else {
            throw std::runtime_error("Unknown argument: " + std::string(arg));
        }
    }

    if (config.frames == 0) {
        throw std::runtime_error("--frames must be greater than zero");
    }
    if (config.width == 0 || config.height == 0) {
        throw std::runtime_error("--width and --height must be greater than zero");
    }
    if (config.renderImage && config.outputPath.empty()) {
        config.outputPath = "out/v1.bmp";
    }

    if (!findProfile(config.profile)) {
        throw std::runtime_error("Unknown profile '" + config.profile + "'. Valid profiles: " + profileKeys());
    }

    return config;
}

void CommandLine::printHelp(std::ostream& out) {
    out
        << "vulkan_render options:\n"
        << "  --profile <" << profileKeys() << ">  Select a staged renderer profile\n"
        << "  --scene <path>             Scene file for profiles that support loading\n"
        << "  --render                   Render a headless image for the selected profile\n"
        << "  --preview                  Open a realtime preview window for v1/v2 software paths\n"
        << "  --output <path>            Output BMP path; implies --render\n"
        << "  --width <n>                Output width for headless renders\n"
        << "  --height <n>               Output height for headless renders\n"
        << "  --frames <n>               Number of graph frames to execute\n"
        << "  --list-devices             Print backend and GPU capability information\n"
        << "  --validate                 Request Vulkan validation when implemented\n"
        << "  --enable-rt                Select v5-rt and use RT if supported\n"
        << "  --require-rt               Fail when RT extensions are unavailable\n"
        << "  --headless                 Avoid window/swapchain work\n"
        << "  --help                     Show this help\n";
}

} // namespace vr
