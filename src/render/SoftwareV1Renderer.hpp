#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

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

struct V1Frame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> bgra;
    V1RenderStats stats;
};

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings);
V1Frame renderSoftwareV1Frame(const V1RenderSettings& settings);

} // namespace vr
