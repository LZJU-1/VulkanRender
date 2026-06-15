#pragma once

#include <cstdint>
#include <array>
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
    bool enableV3Shadows = false;
    bool enableV4Ssao = false;
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

struct GpuPreviewVertex {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 1.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float u = 0.0f;
    float v = 0.0f;
    float textured = 0.0f;
    float roughness = 0.7f;
    float metalness = 0.0f;
    float kind = 0.0f;
    float tx = 1.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    float tw = 1.0f;
};

struct GpuPreviewLight {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float radius = 1.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float intensity = 1.0f;
};

struct GpuPreviewSphereInstance {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float radius = 1.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float roughness = 0.5f;
    float metalness = 0.0f;
    float materialKind = 4.0f;
};

struct GpuPreviewGeometry {
    std::vector<GpuPreviewVertex> vertices;
    V1CameraSettings camera;
    bool manyLightDemo = false;
    std::vector<GpuPreviewLight> lights;
    std::vector<GpuPreviewSphereInstance> sphereInstances;
    struct MaterialTextures {
        std::filesystem::path albedoTexturePath;
        std::filesystem::path normalTexturePath;
        std::filesystem::path roughnessTexturePath;
        std::filesystem::path displacementTexturePath;
    };
    struct Batch {
        std::uint32_t firstVertex = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t materialIndex = 0;
    };
    std::vector<MaterialTextures> materials;
    std::vector<Batch> batches;
    std::filesystem::path environmentBackgroundTexturePath;
    std::filesystem::path environmentDiffuseTexturePath;
    std::array<std::filesystem::path, 5> environmentSpecularTexturePaths;
};

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings);
V1Frame renderSoftwareV1Frame(const V1RenderSettings& settings);
GpuPreviewGeometry buildGpuPreviewGeometry(const V1RenderSettings& settings);

} // namespace vr
