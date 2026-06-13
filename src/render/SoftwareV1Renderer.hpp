#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vr {

struct V1CameraSettings {
    bool enabled = false;
    float eyeX = 0.0f;
    float eyeY = 0.0f;
    float eyeZ = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float upX = 0.0f;
    float upY = 0.0f;
    float upZ = 1.0f;
    float fovY = 0.0f;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
};

struct V1RenderSettings {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameIndex = 0;
    bool enableV2Shading = false;
    V1CameraSettings camera;
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
    V1CameraSettings camera;
    V1RenderStats stats;
};

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings);
V1Frame renderSoftwareV1Frame(const V1RenderSettings& settings);

} // namespace vr
