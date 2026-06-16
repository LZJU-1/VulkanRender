#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>

#include "render/SoftwareV1Renderer.hpp"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace vr {
inline namespace vktypes {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kWindowTimer = 1;
constexpr std::uint32_t kMaterialTextureCount = 4;
constexpr std::uint32_t kEnvironmentSpecularTextureCount = 5;
constexpr std::uint32_t kSharedTextureCount = 2 + kEnvironmentSpecularTextureCount + 1;
constexpr std::uint32_t kEnvironmentBackgroundTexture = 0;
constexpr std::uint32_t kEnvironmentDiffuseTexture = 1;
constexpr std::uint32_t kEnvironmentSpecularTextureBase = 2;
constexpr std::uint32_t kEnvironmentBrdfTextureIndex = kSharedTextureCount - 1;
constexpr std::uint32_t kShadowTileSize = 1024;
constexpr std::uint32_t kShadowAtlasColumns = 4;
constexpr std::uint32_t kShadowAtlasRows = 3;
constexpr std::uint32_t kShadowAtlasWidth = kShadowTileSize * kShadowAtlasColumns;
constexpr std::uint32_t kShadowAtlasHeight = kShadowTileSize * kShadowAtlasRows;
constexpr std::uint32_t kShadowMapCount = 10;
constexpr std::uint32_t kDirectionalShadowTextureBinding = 14;
constexpr std::uint32_t kGBufferAlbedoBinding = 15;
constexpr std::uint32_t kGBufferNormalBinding = 16;
constexpr std::uint32_t kGBufferWorldBinding = 17;
constexpr std::uint32_t kSsaoRawBinding = 18;
constexpr std::uint32_t kSsaoBlurBinding = 19;
constexpr std::uint32_t kV4ManyLightBufferBinding = 20;
constexpr std::uint32_t kV5SceneTlasBinding = 21;
constexpr std::uint32_t kV5HistoryInputBinding = 22;
constexpr std::uint32_t kV5HistoryOutputBinding = 23;
constexpr std::uint32_t kV5ShadowSignalBinding = 24;
constexpr std::uint32_t kV5ReflectionSignalBinding = 25;
constexpr std::uint32_t kV5ShadowHistoryInputBinding = 26;
constexpr std::uint32_t kV5ShadowHistoryOutputBinding = 27;
constexpr std::uint32_t kV5ReflectionHistoryInputBinding = 28;
constexpr std::uint32_t kV5ReflectionHistoryOutputBinding = 29;
constexpr std::uint32_t kV5SurfaceHistoryInputBinding = 30;
constexpr std::uint32_t kV5SurfaceHistoryOutputBinding = 31;
constexpr std::uint32_t kV5ResolvedColorBinding = 32;
constexpr std::uint32_t kV5InternalScale = 2;
constexpr VkFormat kGBufferAlbedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kGBufferNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kGBufferWorldFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat kSsaoFormat = VK_FORMAT_R32_SFLOAT;
constexpr VkFormat kV5HistoryFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kV5ShadowSignalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kV5ReflectionSignalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kV5SurfaceHistoryFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CameraUniform {
    float eyeNear[4]{};
    float rightFar[4]{};
    float upTanHalf[4]{};
    float forwardAspect[4]{};
    float shadowRightExtent[4]{};
    float shadowUpNear[4]{};
    float shadowForwardFar[4]{};
    float shadowCenterBias[4]{};
    float pointPosRadius[4]{};
    float pointColorIntensity[4]{};
    float spotPosInner[4]{};
    float spotDirOuter[4]{};
    float spotColorIntensity[4]{};
    float v3Flags[4]{};
    float v4Flags[4]{};
    float taaJitter[4]{};
    float prevEyeNear[4]{};
    float prevRightFar[4]{};
    float prevUpTanHalf[4]{};
    float prevForwardAspect[4]{};
    float prevTaaJitter[4]{};
};

struct QueueFamilies {
    std::uint32_t graphics = 0;
    std::uint32_t present = 0;
    bool hasGraphics = false;
    bool hasPresent = false;
};

struct TextureResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t mipLevels = 1;
};

struct AccelerationStructureResource {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceAddress address = 0;
};

struct MaterialTextureResources {
    std::array<TextureResource, kMaterialTextureCount> textures;
};

struct RgbaFloat {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct RgFloat {
    float x = 0.0f;
    float y = 0.0f;
};

// --- Math utilities ---

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

inline std::uint32_t mipLevelsFor(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1u;
}

inline VkSampleCountFlagBits chooseSampleCount(VkSampleCountFlags counts) {
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

inline std::uint32_t sampleCountValue(VkSampleCountFlagBits samples) {
    switch (samples) {
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    default: return 1;
    }
}

inline RgbaFloat unpackRgbe(const stbi_uc* rgbe) {
    if (rgbe[3] == 0) {
        return {};
    }
    const float scale = std::ldexp(1.0f, static_cast<int>(rgbe[3]) - 128) / 256.0f;
    return {
        (static_cast<float>(rgbe[0]) + 0.5f) * scale,
        (static_cast<float>(rgbe[1]) + 0.5f) * scale,
        (static_cast<float>(rgbe[2]) + 0.5f) * scale,
        1.0f,
    };
}

inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 0.00001f) {
        return {};
    }
    return v * (1.0f / len);
}

inline float radicalInverseVdc(std::uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

inline std::array<float, 2> hammersley(std::uint32_t i, std::uint32_t count) {
    return {static_cast<float>(i) / static_cast<float>(count), radicalInverseVdc(i)};
}

inline Vec3 importanceSampleGgx(float u1, float u2, float roughness) {
    const float a = roughness * roughness;
    const float phi = 2.0f * kPi * u1;
    const float cosTheta = std::sqrt((1.0f - u2) / (1.0f + (a * a - 1.0f) * u2));
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    return normalize({std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta});
}

inline float geometrySchlickGgx(float ndotv, float roughness) {
    const float a = roughness;
    const float k = (a * a) * 0.5f;
    return ndotv / (ndotv * (1.0f - k) + k);
}

inline float geometrySmith(float ndotv, float ndotl, float roughness) {
    return geometrySchlickGgx(ndotv, roughness) * geometrySchlickGgx(ndotl, roughness);
}

inline RgFloat integrateEnvironmentBrdf(float ndotv, float roughness) {
    const Vec3 view{std::sqrt(std::max(0.0f, 1.0f - ndotv * ndotv)), 0.0f, ndotv};
    float scale = 0.0f;
    float bias = 0.0f;
    constexpr std::uint32_t kSamples = 512;

    for (std::uint32_t i = 0; i < kSamples; ++i) {
        const auto xi = hammersley(i, kSamples);
        const Vec3 halfVector = importanceSampleGgx(xi[0], xi[1], roughness);
        const Vec3 light = normalize(halfVector * (2.0f * dot(view, halfVector)) - view);
        const float ndotl = std::max(light.z, 0.0f);
        const float ndoth = std::max(halfVector.z, 0.0f);
        const float vdoth = std::max(dot(view, halfVector), 0.0f);
        if (ndotl > 0.0f) {
            const float g = geometrySmith(ndotv, ndotl, roughness);
            const float gVis = (g * vdoth) / std::max(0.0001f, ndoth * ndotv);
            const float fc = std::pow(1.0f - vdoth, 5.0f);
            scale += (1.0f - fc) * gVis;
            bias += fc * gVis;
        }
    }
    return {scale / static_cast<float>(kSamples), bias / static_cast<float>(kSamples)};
}

// --- Camera helpers ---

inline Vec3 eyeOf(const V1CameraSettings& camera) {
    return {camera.eyeX, camera.eyeY, camera.eyeZ};
}

inline Vec3 targetOf(const V1CameraSettings& camera) {
    return {camera.targetX, camera.targetY, camera.targetZ};
}

inline Vec3 upOf(const V1CameraSettings& camera) {
    const Vec3 up{camera.upX, camera.upY, camera.upZ};
    return dot(up, up) <= 0.00001f ? Vec3{0.0f, 0.0f, 1.0f} : normalize(up);
}

inline Vec3 forwardFor(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return normalize({cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch)});
}

inline Vec3 rightFor(Vec3 forward, Vec3 upHint = {0.0f, 0.0f, 1.0f}) {
    Vec3 right = normalize(cross(forward, upHint));
    if (dot(right, right) <= 0.00001f) {
        right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    }
    return dot(right, right) <= 0.00001f ? Vec3{1.0f, 0.0f, 0.0f} : right;
}

// --- File / path utilities ---

inline std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open SPIR-V shader: " + path.string());
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("Invalid SPIR-V shader size: " + path.string());
    }
    std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

inline std::filesystem::path executablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer);
}

inline std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) {
    if (relativePath.is_absolute() || std::filesystem::exists(relativePath)) {
        return relativePath;
    }

    std::filesystem::path cursor = executablePath().parent_path();
    while (!cursor.empty()) {
        const std::filesystem::path candidate = cursor / relativePath;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return relativePath;
}

inline std::filesystem::path projectRootPath() {
    std::filesystem::path cursor = executablePath().parent_path();
    while (!cursor.empty()) {
        if (std::filesystem::exists(cursor / "assets") && std::filesystem::exists(cursor / "shaders")) {
            return cursor;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return std::filesystem::current_path();
}

inline std::filesystem::path previewLogPath() {
    const std::filesystem::path path = projectRootPath() / "out/vulkan-preview.log";
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    return path;
}

inline void previewLog(const std::string& message) {
    std::ofstream out(previewLogPath(), std::ios::app);
    out << message << '\n';
}

inline void resetPreviewLog() {
    std::ofstream out(previewLogPath(), std::ios::trunc);
    out << "Vulkan preview log\n";
}

// --- Sphere geometry generator ---

inline std::vector<GpuPreviewVertex> makeUnitSphereVertices(std::uint32_t rings = 16, std::uint32_t segments = 24) {
    const auto spherePoint = [&](std::uint32_t ring, std::uint32_t segment) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float theta = v * kPi;
        const float phi = u * kPi * 2.0f;
        const float sinTheta = std::sin(theta);
        return Vec3{sinTheta * std::cos(phi), sinTheta * std::sin(phi), std::cos(theta)};
    };
    const auto pushVertex = [](std::vector<GpuPreviewVertex>& vertices, Vec3 p, RgFloat uv) {
        Vec3 normal = normalize(p);
        Vec3 tangent = normalize(std::abs(normal.z) < 0.9f ? cross({0.0f, 0.0f, 1.0f}, normal) : cross({0.0f, 1.0f, 0.0f}, normal));
        vertices.push_back({
            p.x, p.y, p.z,
            normal.x, normal.y, normal.z,
            1.0f, 1.0f, 1.0f,
            uv.x, uv.y,
            0.0f,
            0.5f,
            0.0f,
            4.0f,
            tangent.x, tangent.y, tangent.z, 1.0f,
        });
    };
    const auto pushTriangle = [&](std::vector<GpuPreviewVertex>& vertices, Vec3 a, Vec3 b, Vec3 c, RgFloat uvA, RgFloat uvB, RgFloat uvC) {
        pushVertex(vertices, a, uvA);
        pushVertex(vertices, b, uvB);
        pushVertex(vertices, c, uvC);
    };

    std::vector<GpuPreviewVertex> vertices;
    vertices.reserve(static_cast<std::size_t>(rings) * segments * 6u);
    for (std::uint32_t ring = 0; ring < rings; ++ring) {
        for (std::uint32_t segment = 0; segment < segments; ++segment) {
            const std::uint32_t nextSegment = (segment + 1u) % segments;
            const Vec3 p00 = spherePoint(ring, segment);
            const Vec3 p01 = spherePoint(ring, nextSegment);
            const Vec3 p10 = spherePoint(ring + 1u, segment);
            const Vec3 p11 = spherePoint(ring + 1u, nextSegment);
            const RgFloat uv00{static_cast<float>(segment) / static_cast<float>(segments), static_cast<float>(ring) / static_cast<float>(rings)};
            const RgFloat uv01{static_cast<float>(segment + 1u) / static_cast<float>(segments), static_cast<float>(ring) / static_cast<float>(rings)};
            const RgFloat uv10{static_cast<float>(segment) / static_cast<float>(segments), static_cast<float>(ring + 1u) / static_cast<float>(rings)};
            const RgFloat uv11{static_cast<float>(segment + 1u) / static_cast<float>(segments), static_cast<float>(ring + 1u) / static_cast<float>(rings)};
            if (ring == 0) {
                pushTriangle(vertices, p00, p10, p11, uv00, uv10, uv11);
            } else if (ring + 1u == rings) {
                pushTriangle(vertices, p00, p10, p01, uv00, uv10, uv01);
            } else {
                pushTriangle(vertices, p00, p10, p11, uv00, uv10, uv11);
                pushTriangle(vertices, p00, p11, p01, uv00, uv11, uv01);
            }
        }
    }
    return vertices;
}

} // namespace vktypes
} // namespace vr
