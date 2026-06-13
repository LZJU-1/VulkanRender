#pragma once

#include <cstdint>
#include <filesystem>

namespace vr {

struct V1RenderSettings {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameIndex = 0;
    std::filesystem::path scenePath;
    std::filesystem::path outputPath;
};

struct V1RenderStats {
    std::uint32_t objectCount = 0;
    std::uint32_t visibleObjects = 0;
    std::uint32_t trianglesSubmitted = 0;
    std::filesystem::path outputPath;
};

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings);

} // namespace vr

