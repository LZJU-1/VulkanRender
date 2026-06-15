#include "platform/VulkanPreviewWindow.hpp"

#include <stb_image.h>

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace vr {
namespace {

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
constexpr VkFormat kGBufferAlbedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kGBufferNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kGBufferWorldFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat kSsaoFormat = VK_FORMAT_R32_SFLOAT;

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkCreateInstance vkCreateInstance = nullptr;
PFN_vkDestroyInstance vkDestroyInstance = nullptr;
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = nullptr;
PFN_vkCreateDevice vkCreateDevice = nullptr;
PFN_vkDestroyDevice vkDestroyDevice = nullptr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
PFN_vkCreateImageView vkCreateImageView = nullptr;
PFN_vkDestroyImageView vkDestroyImageView = nullptr;
PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
PFN_vkCreateBuffer vkCreateBuffer = nullptr;
PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
PFN_vkAllocateMemory vkAllocateMemory = nullptr;
PFN_vkFreeMemory vkFreeMemory = nullptr;
PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
PFN_vkMapMemory vkMapMemory = nullptr;
PFN_vkUnmapMemory vkUnmapMemory = nullptr;
PFN_vkCreateImage vkCreateImage = nullptr;
PFN_vkDestroyImage vkDestroyImage = nullptr;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
PFN_vkBindImageMemory vkBindImageMemory = nullptr;
PFN_vkCreateSampler vkCreateSampler = nullptr;
PFN_vkDestroySampler vkDestroySampler = nullptr;
PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
PFN_vkCmdBlitImage vkCmdBlitImage = nullptr;
PFN_vkCmdDraw vkCmdDraw = nullptr;
PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
PFN_vkCreateFence vkCreateFence = nullptr;
PFN_vkDestroyFence vkDestroyFence = nullptr;
PFN_vkWaitForFences vkWaitForFences = nullptr;
PFN_vkResetFences vkResetFences = nullptr;
PFN_vkQueueSubmit vkQueueSubmit = nullptr;
PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;

HMODULE vulkanLibraryHandle() {
    static HMODULE library = nullptr;
    if (!library) {
        library = LoadLibraryW(L"vulkan-1.dll");
        if (!library) {
            throw std::runtime_error("Could not load vulkan-1.dll");
        }
    }
    return library;
}

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

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

std::uint32_t mipLevelsFor(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1u;
}

VkSampleCountFlagBits chooseSampleCount(VkSampleCountFlags counts) {
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

std::uint32_t sampleCountValue(VkSampleCountFlagBits samples) {
    switch (samples) {
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    default: return 1;
    }
}

RgbaFloat unpackRgbe(const stbi_uc* rgbe) {
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

float radicalInverseVdc(std::uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

std::array<float, 2> hammersley(std::uint32_t i, std::uint32_t count) {
    return {static_cast<float>(i) / static_cast<float>(count), radicalInverseVdc(i)};
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 0.00001f) {
        return {};
    }
    return v * (1.0f / len);
}

std::vector<GpuPreviewVertex> makeUnitSphereVertices(std::uint32_t rings = 16, std::uint32_t segments = 24) {
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

Vec3 importanceSampleGgx(float u1, float u2, float roughness) {
    const float a = roughness * roughness;
    const float phi = 2.0f * kPi * u1;
    const float cosTheta = std::sqrt((1.0f - u2) / (1.0f + (a * a - 1.0f) * u2));
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    return normalize({std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta});
}

float geometrySchlickGgx(float ndotv, float roughness) {
    const float a = roughness;
    const float k = (a * a) * 0.5f;
    return ndotv / (ndotv * (1.0f - k) + k);
}

float geometrySmith(float ndotv, float ndotl, float roughness) {
    return geometrySchlickGgx(ndotv, roughness) * geometrySchlickGgx(ndotl, roughness);
}

RgFloat integrateEnvironmentBrdf(float ndotv, float roughness) {
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

Vec3 eyeOf(const V1CameraSettings& camera) {
    return {camera.eyeX, camera.eyeY, camera.eyeZ};
}

Vec3 targetOf(const V1CameraSettings& camera) {
    return {camera.targetX, camera.targetY, camera.targetZ};
}

Vec3 forwardFor(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return normalize({cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch)});
}

Vec3 rightFor(Vec3 forward) {
    const Vec3 right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    return dot(right, right) <= 0.00001f ? Vec3{1.0f, 0.0f, 0.0f} : right;
}

std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) {
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

std::filesystem::path executablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer);
}

std::filesystem::path resolveProjectPath(const std::filesystem::path& relativePath) {
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

std::filesystem::path projectRootPath() {
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

std::filesystem::path previewLogPath() {
    const std::filesystem::path path = projectRootPath() / "out/vulkan-preview.log";
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    return path;
}

void previewLog(const std::string& message) {
    std::ofstream out(previewLogPath(), std::ios::app);
    out << message << '\n';
}

void resetPreviewLog() {
    std::ofstream out(previewLogPath(), std::ios::trunc);
    out << "Vulkan preview log\n";
}

void require(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed");
    }
}

template <class T>
void loadGlobal(HMODULE library, T& target, const char* name) {
    target = reinterpret_cast<T>(GetProcAddress(library, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan global function: ") + name);
    }
}

template <class T>
void loadInstance(VkInstance instance, T& target, const char* name) {
    target = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan instance function: ") + name);
    }
}

template <class T>
void loadDevice(VkDevice device, T& target, const char* name) {
    target = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
    if (!target) {
        throw std::runtime_error(std::string("Could not load Vulkan device function: ") + name);
    }
}

void loadVulkanLibrary() {
    static bool loaded = false;
    if (loaded) {
        return;
    }
    SetEnvironmentVariableW(
        L"VK_LOADER_LAYERS_DISABLE",
        L"VK_LAYER_VALVE_steam_overlay,VK_LAYER_VALVE_steam_fossilize"
    );
    SetEnvironmentVariableW(L"DISABLE_LAYER_NV_OPTIMUS_1", L"1");
    SetEnvironmentVariableW(L"DISABLE_LAYER_NV_PRESENT_1", L"1");
    HMODULE library = vulkanLibraryHandle();
    loadGlobal(library, vkGetInstanceProcAddr, "vkGetInstanceProcAddr");
    loadGlobal(library, vkCreateInstance, "vkCreateInstance");
    loaded = true;
}

std::uint32_t findMemoryType(VkPhysicalDevice physicalDevice, std::uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    throw std::runtime_error("No compatible Vulkan memory type found");
}

class VulkanGpuRenderer {
public:
    VulkanGpuRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height, const GpuPreviewGeometry& geometry, bool enableV3Shadows, bool enableV4Ssao)
        : hwnd_(hwnd), width_(width), height_(height), geometry_(geometry), enableV3Shadows_(enableV3Shadows), enableV4Ssao_(enableV4Ssao) {
        previewLog("VulkanGpuRenderer: loadVulkanLibrary");
        loadVulkanLibrary();
        previewLog("VulkanGpuRenderer: createInstance");
        createInstance();
        previewLog("VulkanGpuRenderer: loadInstanceFunctions");
        loadInstanceFunctions();
        previewLog("VulkanGpuRenderer: createSurface");
        createSurface();
        previewLog("VulkanGpuRenderer: selectDevice");
        selectDevice();
        previewLog("VulkanGpuRenderer: createDevice");
        createDevice();
        previewLog("VulkanGpuRenderer: loadDeviceFunctions");
        loadDeviceFunctions();
        previewLog("VulkanGpuRenderer: fetchDeviceQueues");
        fetchDeviceQueues();
        previewLog("VulkanGpuRenderer: createSwapchain");
        createSwapchain();
        previewLog("VulkanGpuRenderer: createRenderPass");
        createRenderPass();
        previewLog("VulkanGpuRenderer: createShadowRenderPass");
        createShadowRenderPass();
        if (enableV4Ssao_) {
            previewLog("VulkanGpuRenderer: createGBufferRenderPass");
            createGBufferRenderPass();
            previewLog("VulkanGpuRenderer: createSsaoRenderPass");
            createSsaoRenderPass();
        }
        previewLog("VulkanGpuRenderer: createDepthResources");
        createDepthResources();
        previewLog("VulkanGpuRenderer: createShadowResources");
        createShadowResources();
        if (enableV4Ssao_) {
            previewLog("VulkanGpuRenderer: createGBufferResources");
            createGBufferResources();
            previewLog("VulkanGpuRenderer: createSsaoResources");
            createSsaoResources();
        }
        previewLog("VulkanGpuRenderer: createMsaaColorResources");
        createMsaaColorResources();
        previewLog("VulkanGpuRenderer: createFramebuffers");
        createFramebuffers();
        previewLog("VulkanGpuRenderer: createBuffers");
        createBuffers();
        previewLog("VulkanGpuRenderer: createTextureResources");
        createTextureResources();
        previewLog("VulkanGpuRenderer: createDescriptors");
        createDescriptors();
        if (enableV4Ssao_) {
            previewLog("VulkanGpuRenderer: createV4ComposeDescriptors");
            createV4ComposeDescriptors();
        }
        previewLog(
            "VulkanGpuRenderer: materialSets=" + std::to_string(materialTextures_.size())
            + " batches=" + std::to_string(geometry_.batches.size())
            + " lights=" + std::to_string(geometry_.lights.size())
            + " sphereInstances=" + std::to_string(geometry_.sphereInstances.size())
        );
        previewLog("VulkanGpuRenderer: createPipeline");
        createPipeline();
        previewLog("VulkanGpuRenderer: createCommands");
        createCommands();
        previewLog("VulkanGpuRenderer: createSync");
        createSync();
        previewLog("VulkanGpuRenderer: ready");
    }

    ~VulkanGpuRenderer() {
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
        }
        if (device_ != VK_NULL_HANDLE && imageAvailable_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, imageAvailable_, nullptr);
        if (device_ != VK_NULL_HANDLE && renderFinished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, renderFinished_, nullptr);
        if (device_ != VK_NULL_HANDLE && inFlight_ != VK_NULL_HANDLE) vkDestroyFence(device_, inFlight_, nullptr);
        if (device_ != VK_NULL_HANDLE && commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, gbufferFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, ssaoFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurFramebuffer_ != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, ssaoBlurFramebuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4ComposePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, v4ComposePipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, ssaoBlurPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, ssaoPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && instancedGBufferPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, instancedGBufferPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, gbufferPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, shadowPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && skyPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, skyPipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4ComposePipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, v4ComposePipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4DescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, v4DescriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && v4DescriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, v4DescriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (device_ != VK_NULL_HANDLE && descriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        if (device_ != VK_NULL_HANDLE && textureSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, textureSampler_, nullptr);
        for (MaterialTextureResources& material : materialTextures_) {
            for (TextureResource& texture : material.textures) {
                if (device_ != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
                if (device_ != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
                if (device_ != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
            }
        }
        for (TextureResource& texture : sharedTextures_) {
            if (device_ != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) vkDestroyImageView(device_, texture.view, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, texture.image, nullptr);
            if (device_ != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, texture.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && uniformBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, uniformBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && uniformMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, uniformMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && lightBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, lightBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && lightMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, lightMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereInstanceBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, sphereInstanceBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereInstanceMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, sphereInstanceMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, sphereVertexBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && sphereVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, sphereVertexMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        if (device_ != VK_NULL_HANDLE && vertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexMemory_, nullptr);
        for (VkFramebuffer framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, msaaColorView_, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, msaaColorImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && msaaColorMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, msaaColorMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, depthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, depthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && depthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, depthMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, shadowDepthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, shadowDepthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowDepthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, shadowDepthMemory_, nullptr);
        for (TextureResource& target : gbufferTargets_) {
            if (device_ != VK_NULL_HANDLE && target.view != VK_NULL_HANDLE) vkDestroyImageView(device_, target.view, nullptr);
            if (device_ != VK_NULL_HANDLE && target.image != VK_NULL_HANDLE) vkDestroyImage(device_, target.image, nullptr);
            if (device_ != VK_NULL_HANDLE && target.memory != VK_NULL_HANDLE) vkFreeMemory(device_, target.memory, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, ssaoTarget_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.image != VK_NULL_HANDLE) vkDestroyImage(device_, ssaoTarget_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoTarget_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, ssaoTarget_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.view != VK_NULL_HANDLE) vkDestroyImageView(device_, ssaoBlurTarget_.view, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.image != VK_NULL_HANDLE) vkDestroyImage(device_, ssaoBlurTarget_.image, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoBlurTarget_.memory != VK_NULL_HANDLE) vkFreeMemory(device_, ssaoBlurTarget_.memory, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, gbufferDepthView_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, gbufferDepthImage_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferDepthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, gbufferDepthMemory_, nullptr);
        if (device_ != VK_NULL_HANDLE && ssaoRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, ssaoRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && gbufferRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, gbufferRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && shadowRenderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE && renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
        for (VkImageView view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
        if (device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    void draw(const V1CameraSettings& camera, std::uint32_t v4DebugMode = 0) {
        const bool logFrame = frameIndex_ < 4;
        if (logFrame) previewLog("draw: wait fence");
        vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        if (logFrame) previewLog("draw: reset fence");
        vkResetFences(device_, 1, &inFlight_);

        std::uint32_t imageIndex = 0;
        if (logFrame) previewLog("draw: acquire");
        VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &imageIndex);
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
            previewLog("draw: acquire returned non-success");
            return;
        }

        if (logFrame) previewLog("draw: update uniform");
        updateUniform(camera, v4DebugMode);
        if (logFrame) previewLog("draw: record command buffer");
        recordCommandBuffer(commandBuffer_, imageIndex);

        if (logFrame) previewLog("draw: submit");
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable_;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer_;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished_;
        require(vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_), "vkQueueSubmit");

        if (logFrame) previewLog("draw: present");
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished_;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &imageIndex;
        vkQueuePresentKHR(presentQueue_, &present);
        if (logFrame) previewLog("draw: done");
        ++frameIndex_;
    }

    void updateGeometry(const GpuPreviewGeometry& geometry) {
        const VkDeviceSize bytes = sizeof(GpuPreviewVertex) * geometry.vertices.size();
        if (bytes > vertexBytes_) {
            throw std::runtime_error("Animated geometry grew beyond the Vulkan preview vertex buffer");
        }
        if (bytes == 0) {
            vertexCount_ = 0;
            return;
        }
        vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        void* mapped = nullptr;
        require(vkMapMemory(device_, vertexMemory_, 0, bytes, 0, &mapped), "vkMapMemory(animated vertex)");
        std::memcpy(mapped, geometry.vertices.data(), static_cast<std::size_t>(bytes));
        vkUnmapMemory(device_, vertexMemory_);
        vertexCount_ = static_cast<std::uint32_t>(geometry.vertices.size());
    }

private:
    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "LZJU VulkanRender GPU Preview";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
        appInfo.pEngineName = "LZJU VulkanRender";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;
        require(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
    }

    void loadInstanceFunctions() {
        loadInstance(instance_, vkDestroyInstance, "vkDestroyInstance");
        loadInstance(instance_, vkCreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR");
        loadInstance(instance_, vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
        loadInstance(instance_, vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
        loadInstance(instance_, vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
        loadInstance(instance_, vkGetPhysicalDeviceFeatures, "vkGetPhysicalDeviceFeatures");
        loadInstance(instance_, vkGetPhysicalDeviceFormatProperties, "vkGetPhysicalDeviceFormatProperties");
        loadInstance(instance_, vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        loadInstance(instance_, vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        loadInstance(instance_, vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
        loadInstance(instance_, vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
        loadInstance(instance_, vkCreateDevice, "vkCreateDevice");
        loadInstance(instance_, vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
    }

    void loadDeviceFunctions() {
        loadDevice(device_, vkDestroyDevice, "vkDestroyDevice");
        loadDevice(device_, vkGetDeviceQueue, "vkGetDeviceQueue");
        loadDevice(device_, vkDeviceWaitIdle, "vkDeviceWaitIdle");
        loadDevice(device_, vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
        loadDevice(device_, vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
        loadDevice(device_, vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
        loadDevice(device_, vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
        loadDevice(device_, vkQueuePresentKHR, "vkQueuePresentKHR");
        loadDevice(device_, vkCreateImageView, "vkCreateImageView");
        loadDevice(device_, vkDestroyImageView, "vkDestroyImageView");
        loadDevice(device_, vkCreateRenderPass, "vkCreateRenderPass");
        loadDevice(device_, vkDestroyRenderPass, "vkDestroyRenderPass");
        loadDevice(device_, vkCreateFramebuffer, "vkCreateFramebuffer");
        loadDevice(device_, vkDestroyFramebuffer, "vkDestroyFramebuffer");
        loadDevice(device_, vkCreateShaderModule, "vkCreateShaderModule");
        loadDevice(device_, vkDestroyShaderModule, "vkDestroyShaderModule");
        loadDevice(device_, vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
        loadDevice(device_, vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
        loadDevice(device_, vkCreateDescriptorPool, "vkCreateDescriptorPool");
        loadDevice(device_, vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
        loadDevice(device_, vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
        loadDevice(device_, vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
        loadDevice(device_, vkCreatePipelineLayout, "vkCreatePipelineLayout");
        loadDevice(device_, vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
        loadDevice(device_, vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
        loadDevice(device_, vkDestroyPipeline, "vkDestroyPipeline");
        loadDevice(device_, vkCreateBuffer, "vkCreateBuffer");
        loadDevice(device_, vkDestroyBuffer, "vkDestroyBuffer");
        loadDevice(device_, vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
        loadDevice(device_, vkAllocateMemory, "vkAllocateMemory");
        loadDevice(device_, vkFreeMemory, "vkFreeMemory");
        loadDevice(device_, vkBindBufferMemory, "vkBindBufferMemory");
        loadDevice(device_, vkMapMemory, "vkMapMemory");
        loadDevice(device_, vkUnmapMemory, "vkUnmapMemory");
        loadDevice(device_, vkCreateImage, "vkCreateImage");
        loadDevice(device_, vkDestroyImage, "vkDestroyImage");
        loadDevice(device_, vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
        loadDevice(device_, vkBindImageMemory, "vkBindImageMemory");
        loadDevice(device_, vkCreateSampler, "vkCreateSampler");
        loadDevice(device_, vkDestroySampler, "vkDestroySampler");
        loadDevice(device_, vkCreateCommandPool, "vkCreateCommandPool");
        loadDevice(device_, vkDestroyCommandPool, "vkDestroyCommandPool");
        loadDevice(device_, vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
        loadDevice(device_, vkResetCommandBuffer, "vkResetCommandBuffer");
        loadDevice(device_, vkBeginCommandBuffer, "vkBeginCommandBuffer");
        loadDevice(device_, vkEndCommandBuffer, "vkEndCommandBuffer");
        loadDevice(device_, vkFreeCommandBuffers, "vkFreeCommandBuffers");
        loadDevice(device_, vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
        loadDevice(device_, vkCmdEndRenderPass, "vkCmdEndRenderPass");
        loadDevice(device_, vkCmdBindPipeline, "vkCmdBindPipeline");
        loadDevice(device_, vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
        loadDevice(device_, vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
        loadDevice(device_, vkCmdSetViewport, "vkCmdSetViewport");
        loadDevice(device_, vkCmdSetScissor, "vkCmdSetScissor");
        loadDevice(device_, vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
        loadDevice(device_, vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
        loadDevice(device_, vkCmdBlitImage, "vkCmdBlitImage");
        loadDevice(device_, vkCmdDraw, "vkCmdDraw");
        loadDevice(device_, vkCreateSemaphore, "vkCreateSemaphore");
        loadDevice(device_, vkDestroySemaphore, "vkDestroySemaphore");
        loadDevice(device_, vkCreateFence, "vkCreateFence");
        loadDevice(device_, vkDestroyFence, "vkDestroyFence");
        loadDevice(device_, vkWaitForFences, "vkWaitForFences");
        loadDevice(device_, vkResetFences, "vkResetFences");
        loadDevice(device_, vkQueueSubmit, "vkQueueSubmit");
        loadDevice(device_, vkQueueWaitIdle, "vkQueueWaitIdle");
    }

    void createSurface() {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = GetModuleHandleW(nullptr);
        createInfo.hwnd = hwnd_;
        require(vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_), "vkCreateWin32SurfaceKHR");
    }

    QueueFamilies queueFamiliesFor(VkPhysicalDevice device) const {
        std::uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
        QueueFamilies out;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                out.graphics = i;
                out.hasGraphics = true;
            }
            VkBool32 present = VK_FALSE;
            if (surface_ != VK_NULL_HANDLE) {
                require(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present), "vkGetPhysicalDeviceSurfaceSupportKHR");
            }
            if (present) {
                out.present = i;
                out.hasPresent = true;
            }
            if (out.hasGraphics && out.hasPresent) {
                break;
            }
        }
        return out;
    }

    void selectDevice() {
        std::uint32_t count = 0;
        require(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
        if (count == 0) {
            throw std::runtime_error("No Vulkan GPU found");
        }
        std::vector<VkPhysicalDevice> devices(count);
        require(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

        for (VkPhysicalDevice device : devices) {
            const QueueFamilies families = queueFamiliesFor(device);
            if (!families.hasGraphics || !families.hasPresent) {
                continue;
            }
            std::uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
            const bool hasSwapchain = std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties& ext) {
                return std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });
            if (!hasSwapchain) {
                continue;
            }
            VkSurfaceCapabilitiesKHR caps{};
            require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
            std::uint32_t formatCount = 0;
            require(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
            if (formatCount == 0) {
                continue;
            }
            physicalDevice_ = device;
            queueFamilies_ = families;
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(device, &features);
            gpuName_ = properties.deviceName;
            msaaSamples_ = chooseSampleCount(
                properties.limits.framebufferColorSampleCounts
                & properties.limits.framebufferDepthSampleCounts
            );
            samplerAnisotropy_ = features.samplerAnisotropy == VK_TRUE;
            maxSamplerAnisotropy_ = samplerAnisotropy_ ? std::min(16.0f, properties.limits.maxSamplerAnisotropy) : 1.0f;
            previewLog(
                "selectDevice: " + gpuName_
                + " graphicsQueue=" + std::to_string(queueFamilies_.graphics)
                + " presentQueue=" + std::to_string(queueFamilies_.present)
                + " msaa=" + std::to_string(sampleCountValue(msaaSamples_)) + "x"
                + " anisotropy=" + (samplerAnisotropy_ ? std::to_string(maxSamplerAnisotropy_) + "x" : "off")
            );
            return;
        }
        throw std::runtime_error("No Vulkan GPU supports graphics, present, and swapchain");
    }

    void createDevice() {
        const float priority = 1.0f;
        std::array<std::uint32_t, 2> uniqueFamilies{queueFamilies_.graphics, queueFamilies_.present};
        std::vector<VkDeviceQueueCreateInfo> queues;
        queues.reserve(queueFamilies_.graphics == queueFamilies_.present ? 1u : 2u);
        for (std::uint32_t family : uniqueFamilies) {
            if (!queues.empty() && queues.front().queueFamilyIndex == family) {
                continue;
            }
            VkDeviceQueueCreateInfo queue{};
            queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            queues.push_back(queue);
        }

        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = extensions;
        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.samplerAnisotropy = samplerAnisotropy_ ? VK_TRUE : VK_FALSE;
        createInfo.pEnabledFeatures = &enabledFeatures;
        require(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");
    }

    void fetchDeviceQueues() {
        vkGetDeviceQueue(device_, queueFamilies_.graphics, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, queueFamilies_.present, 0, &presentQueue_);
    }

    void createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

        std::uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
        swapchainFormat_ = formats.empty() ? VK_FORMAT_B8G8R8A8_UNORM : formats.front().format;
        VkColorSpaceKHR colorSpace = formats.empty() ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : formats.front().colorSpace;
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapchainFormat_ = format.format;
                colorSpace = format.colorSpace;
            }
        }

        swapchainExtent_ = caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()
            ? caps.currentExtent
            : VkExtent2D{std::clamp(width_, caps.minImageExtent.width, caps.maxImageExtent.width), std::clamp(height_, caps.minImageExtent.height, caps.maxImageExtent.height)};

        std::uint32_t imageCount = std::max(2u, caps.minImageCount);
        if (caps.maxImageCount > 0) {
            imageCount = std::min(imageCount, caps.maxImageCount);
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = swapchainFormat_;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = swapchainExtent_;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (queueFamilies_.graphics != queueFamilies_.present) {
            std::uint32_t indices[] = {queueFamilies_.graphics, queueFamilies_.present};
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = indices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = caps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        require(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "vkCreateSwapchainKHR");

        std::uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, nullptr);
        swapchainImages_.resize(actualCount);
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, swapchainImages_.data());
        swapchainImageViews_.reserve(swapchainImages_.size());
        for (VkImage image : swapchainImages_) {
            swapchainImageViews_.push_back(createImageView(image, swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT));
        }
    }

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        std::uint32_t mipLevels = 1,
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
        std::uint32_t layers = 1
    ) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = viewType;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = mipLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = layers;
        VkImageView view = VK_NULL_HANDLE;
        require(vkCreateImageView(device_, &createInfo, nullptr, &view), "vkCreateImageView");
        return view;
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapchainFormat_;
        color.samples = msaaSamples_;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = msaaSamples_ == VK_SAMPLE_COUNT_1_BIT ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = msaaSamples_ == VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = msaaSamples_;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;

        if (msaaSamples_ == VK_SAMPLE_COUNT_1_BIT) {
            std::array<VkAttachmentDescription, 2> attachments{color, depth};
            createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            require(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_), "vkCreateRenderPass");
            return;
        }

        VkAttachmentDescription resolve{};
        resolve.format = swapchainFormat_;
        resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference resolveRef{};
        resolveRef.attachment = 2;
        resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.pResolveAttachments = &resolveRef;

        std::array<VkAttachmentDescription, 3> attachments{color, depth, resolve};
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        require(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_), "vkCreateRenderPass");
    }

    void createShadowRenderPass() {
        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 0;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &depth;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.data();
        require(vkCreateRenderPass(device_, &createInfo, nullptr, &shadowRenderPass_), "vkCreateRenderPass(shadow)");
    }

    void createGBufferRenderPass() {
        VkAttachmentDescription albedo{};
        albedo.format = kGBufferAlbedoFormat;
        albedo.samples = VK_SAMPLE_COUNT_1_BIT;
        albedo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        albedo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        albedo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        albedo.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription normal{};
        normal.format = kGBufferNormalFormat;
        normal.samples = VK_SAMPLE_COUNT_1_BIT;
        normal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        normal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        normal.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        normal.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription world{};
        world.format = kGBufferWorldFormat;
        world.samples = VK_SAMPLE_COUNT_1_BIT;
        world.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        world.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        world.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        world.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentReference, 3> colorRefs{};
        colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        colorRefs[2] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{};
        depthRef.attachment = 3;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<std::uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        std::array<VkAttachmentDescription, 4> attachments{albedo, normal, world, depth};
        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.data();
        require(vkCreateRenderPass(device_, &createInfo, nullptr, &gbufferRenderPass_), "vkCreateRenderPass(gbuffer)");
    }

    void createSsaoRenderPass() {
        VkAttachmentDescription ssao{};
        ssao.format = kSsaoFormat;
        ssao.samples = VK_SAMPLE_COUNT_1_BIT;
        ssao.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ssao.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ssao.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ssao.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &ssao;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.data();
        require(vkCreateRenderPass(device_, &createInfo, nullptr, &ssaoRenderPass_), "vkCreateRenderPass(ssao)");
    }

    void createDepthResources() {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_D32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image.samples = msaaSamples_;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &depthImage_), "vkCreateImage(depth)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, depthImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &depthMemory_), "vkAllocateMemory(depth)");
        require(vkBindImageMemory(device_, depthImage_, depthMemory_, 0), "vkBindImageMemory(depth)");
        depthView_ = createImageView(depthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void createShadowResources() {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {kShadowAtlasWidth, kShadowAtlasHeight, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_D32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &shadowDepthImage_), "vkCreateImage(shadow depth)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, shadowDepthImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &shadowDepthMemory_), "vkAllocateMemory(shadow depth)");
        require(vkBindImageMemory(device_, shadowDepthImage_, shadowDepthMemory_, 0), "vkBindImageMemory(shadow depth)");
        shadowDepthView_ = createImageView(shadowDepthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = shadowRenderPass_;
        framebuffer.attachmentCount = 1;
        framebuffer.pAttachments = &shadowDepthView_;
        framebuffer.width = kShadowAtlasWidth;
        framebuffer.height = kShadowAtlasHeight;
        framebuffer.layers = 1;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &shadowFramebuffer_), "vkCreateFramebuffer(shadow)");

    }

    void createColorAttachment(TextureResource& target, VkFormat format, const char* label) {
        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = format;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &target.image), label);

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, target.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &target.memory), "vkAllocateMemory(gbuffer)");
        require(vkBindImageMemory(device_, target.image, target.memory, 0), "vkBindImageMemory(gbuffer)");
        target.view = createImageView(target.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createGBufferResources() {
        createColorAttachment(gbufferTargets_[0], kGBufferAlbedoFormat, "vkCreateImage(gbuffer albedo)");
        createColorAttachment(gbufferTargets_[1], kGBufferNormalFormat, "vkCreateImage(gbuffer normal)");
        createColorAttachment(gbufferTargets_[2], kGBufferWorldFormat, "vkCreateImage(gbuffer world)");

        VkImageCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depth.imageType = VK_IMAGE_TYPE_2D;
        depth.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        depth.mipLevels = 1;
        depth.arrayLayers = 1;
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.tiling = VK_IMAGE_TILING_OPTIMAL;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &depth, nullptr, &gbufferDepthImage_), "vkCreateImage(gbuffer depth)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, gbufferDepthImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &gbufferDepthMemory_), "vkAllocateMemory(gbuffer depth)");
        require(vkBindImageMemory(device_, gbufferDepthImage_, gbufferDepthMemory_, 0), "vkBindImageMemory(gbuffer depth)");
        gbufferDepthView_ = createImageView(gbufferDepthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

        std::array<VkImageView, 4> attachments{
            gbufferTargets_[0].view,
            gbufferTargets_[1].view,
            gbufferTargets_[2].view,
            gbufferDepthView_,
        };
        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = gbufferRenderPass_;
        framebuffer.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        framebuffer.pAttachments = attachments.data();
        framebuffer.width = swapchainExtent_.width;
        framebuffer.height = swapchainExtent_.height;
        framebuffer.layers = 1;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &gbufferFramebuffer_), "vkCreateFramebuffer(gbuffer)");
    }

    void createSsaoResources() {
        createColorAttachment(ssaoTarget_, kSsaoFormat, "vkCreateImage(ssao raw)");
        createColorAttachment(ssaoBlurTarget_, kSsaoFormat, "vkCreateImage(ssao blur)");

        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = ssaoRenderPass_;
        framebuffer.attachmentCount = 1;
        framebuffer.width = swapchainExtent_.width;
        framebuffer.height = swapchainExtent_.height;
        framebuffer.layers = 1;

        framebuffer.pAttachments = &ssaoTarget_.view;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &ssaoFramebuffer_), "vkCreateFramebuffer(ssao)");

        framebuffer.pAttachments = &ssaoBlurTarget_.view;
        require(vkCreateFramebuffer(device_, &framebuffer, nullptr, &ssaoBlurFramebuffer_), "vkCreateFramebuffer(ssao blur)");
    }

    void createMsaaColorResources() {
        if (msaaSamples_ == VK_SAMPLE_COUNT_1_BIT) {
            return;
        }

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = swapchainFormat_;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image.samples = msaaSamples_;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &msaaColorImage_), "vkCreateImage(msaa color)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, msaaColorImage_, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &msaaColorMemory_), "vkAllocateMemory(msaa color)");
        require(vkBindImageMemory(device_, msaaColorImage_, msaaColorMemory_, 0), "vkBindImageMemory(msaa color)");
        msaaColorView_ = createImageView(msaaColorImage_, swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createFramebuffers() {
        framebuffers_.reserve(swapchainImageViews_.size());
        for (VkImageView view : swapchainImageViews_) {
            std::array<VkImageView, 3> attachmentsWithMsaa{msaaColorView_, depthView_, view};
            std::array<VkImageView, 2> attachmentsWithoutMsaa{view, depthView_};
            const bool useMsaa = msaaSamples_ != VK_SAMPLE_COUNT_1_BIT;
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass_;
            createInfo.attachmentCount = useMsaa
                ? static_cast<std::uint32_t>(attachmentsWithMsaa.size())
                : static_cast<std::uint32_t>(attachmentsWithoutMsaa.size());
            createInfo.pAttachments = useMsaa ? attachmentsWithMsaa.data() : attachmentsWithoutMsaa.data();
            createInfo.width = swapchainExtent_.width;
            createInfo.height = swapchainExtent_.height;
            createInfo.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            require(vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            framebuffers_.push_back(framebuffer);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateBuffer(device_, &createInfo, nullptr, &buffer), "vkCreateBuffer");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, buffer, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(
            physicalDevice_,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        require(vkAllocateMemory(device_, &allocate, nullptr, &memory), "vkAllocateMemory(buffer)");
        require(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
    }

    void createBuffers() {
        vertexCount_ = static_cast<std::uint32_t>(geometry_.vertices.size());
        vertexBytes_ = std::max<VkDeviceSize>(1, sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        createBuffer(vertexBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexMemory_);
        void* mapped = nullptr;
        require(vkMapMemory(device_, vertexMemory_, 0, vertexBytes_, 0, &mapped), "vkMapMemory(vertex)");
        std::memcpy(mapped, geometry_.vertices.data(), sizeof(GpuPreviewVertex) * geometry_.vertices.size());
        vkUnmapMemory(device_, vertexMemory_);

        createBuffer(sizeof(CameraUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffer_, uniformMemory_);

        lightCount_ = static_cast<std::uint32_t>(geometry_.lights.size());
        lightBytes_ = std::max<VkDeviceSize>(sizeof(GpuPreviewLight), sizeof(GpuPreviewLight) * geometry_.lights.size());
        createBuffer(lightBytes_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, lightBuffer_, lightMemory_);
        if (!geometry_.lights.empty()) {
            void* lightMapped = nullptr;
            require(vkMapMemory(device_, lightMemory_, 0, lightBytes_, 0, &lightMapped), "vkMapMemory(light buffer)");
            std::memcpy(lightMapped, geometry_.lights.data(), sizeof(GpuPreviewLight) * geometry_.lights.size());
            vkUnmapMemory(device_, lightMemory_);
        }

        sphereInstanceCount_ = static_cast<std::uint32_t>(geometry_.sphereInstances.size());
        if (!geometry_.sphereInstances.empty()) {
            const std::vector<GpuPreviewVertex> unitSphere = makeUnitSphereVertices(16, 24);
            sphereVertexCount_ = static_cast<std::uint32_t>(unitSphere.size());
            sphereVertexBytes_ = sizeof(GpuPreviewVertex) * unitSphere.size();
            createBuffer(sphereVertexBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sphereVertexBuffer_, sphereVertexMemory_);
            void* sphereMapped = nullptr;
            require(vkMapMemory(device_, sphereVertexMemory_, 0, sphereVertexBytes_, 0, &sphereMapped), "vkMapMemory(sphere vertex)");
            std::memcpy(sphereMapped, unitSphere.data(), static_cast<std::size_t>(sphereVertexBytes_));
            vkUnmapMemory(device_, sphereVertexMemory_);

            sphereInstanceBytes_ = sizeof(GpuPreviewSphereInstance) * geometry_.sphereInstances.size();
            createBuffer(sphereInstanceBytes_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sphereInstanceBuffer_, sphereInstanceMemory_);
            void* instanceMapped = nullptr;
            require(vkMapMemory(device_, sphereInstanceMemory_, 0, sphereInstanceBytes_, 0, &instanceMapped), "vkMapMemory(sphere instance)");
            std::memcpy(instanceMapped, geometry_.sphereInstances.data(), static_cast<std::size_t>(sphereInstanceBytes_));
            vkUnmapMemory(device_, sphereInstanceMemory_);
        }
    }

    VkCommandBuffer beginOneTimeCommands() {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = queueFamilies_.graphics;
        require(vkCreateCommandPool(device_, &pool, nullptr, &uploadCommandPool_), "vkCreateCommandPool(upload)");

        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = uploadCommandPool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        require(vkAllocateCommandBuffers(device_, &allocate, &commandBuffer), "vkAllocateCommandBuffers(upload)");

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer(upload)");
        return commandBuffer;
    }

    void endOneTimeCommands(VkCommandBuffer commandBuffer) {
        require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(upload)");
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        require(vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
        require(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(upload)");
        vkFreeCommandBuffers(device_, uploadCommandPool_, 1, &commandBuffer);
        vkDestroyCommandPool(device_, uploadCommandPool_, nullptr);
        uploadCommandPool_ = VK_NULL_HANDLE;
    }

    void transitionImage(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t mipLevels = 1
    ) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels) {
        std::int32_t mipWidth = static_cast<std::int32_t>(width);
        std::int32_t mipHeight = static_cast<std::int32_t>(height);

        for (std::uint32_t i = 1; i < mipLevels; ++i) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            vkCmdBlitImage(
                commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
            mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );
    }

    void createSampledTexture(
        const std::filesystem::path& path,
        const std::array<stbi_uc, 4>& fallback,
        TextureResource& texture
    ) {
        int width = 1;
        int height = 1;
        int channels = 4;
        stbi_uc* loaded = nullptr;
        const stbi_uc* pixels = fallback.data();
        if (!path.empty()) {
            loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (loaded) {
                pixels = loaded;
                channels = 4;
            } else {
                previewLog("createTextureResources: failed to load " + path.string());
            }
        }

        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
        const bool canLinearBlit = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
        texture.mipLevels = canLinearBlit
            ? mipLevelsFor(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))
            : 1u;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(texture staging)");
        std::memcpy(mapped, pixels, static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);
        if (loaded) {
            stbi_image_free(loaded);
        }

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        image.mipLevels = texture.mipLevels;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(texture)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(texture)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(texture)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        if (texture.mipLevels > 1) {
            generateMipmaps(
                commandBuffer,
                texture.image,
                static_cast<std::uint32_t>(width),
                static_cast<std::uint32_t>(height),
                texture.mipLevels
            );
        } else {
            transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, texture.mipLevels);
    }

    void createRgbeCubeTexture(const std::filesystem::path& path, const RgbaFloat& fallback, TextureResource& texture) {
        int width = 1;
        int height = 6;
        int channels = 4;
        stbi_uc* loaded = nullptr;
        std::vector<RgbaFloat> pixels;

        if (!path.empty()) {
            loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (!loaded) {
                previewLog("createTextureResources: failed to load cube " + path.string());
            }
        }

        if (loaded && width > 0 && height == width * 6) {
            pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                pixels[i] = unpackRgbe(loaded + i * 4u);
            }
        } else {
            if (loaded) {
                previewLog("createTextureResources: cube texture must be a vertical 6-face strip: " + path.string());
                stbi_image_free(loaded);
                loaded = nullptr;
            }
            width = 1;
            height = 6;
            pixels.assign(6, fallback);
        }
        if (loaded) {
            stbi_image_free(loaded);
        }

        const std::uint32_t faceSize = static_cast<std::uint32_t>(width);
        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(pixels.size()) * sizeof(RgbaFloat);
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(cube staging)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {faceSize, faceSize, 1};
        image.mipLevels = 1;
        image.arrayLayers = 6;
        image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(cube texture)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(cube texture)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(cube texture)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 6;
        copy.imageExtent = {faceSize, faceSize, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.mipLevels = 1;
        texture.view = createImageView(
            texture.image,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            VK_IMAGE_VIEW_TYPE_CUBE,
            6
        );
    }

    void createEnvironmentBrdfTexture(TextureResource& texture) {
        constexpr std::uint32_t size = 128;
        std::vector<RgFloat> pixels(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        for (std::uint32_t y = 0; y < size; ++y) {
            for (std::uint32_t x = 0; x < size; ++x) {
                const float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                pixels[static_cast<std::size_t>(y) * size + x] = integrateEnvironmentBrdf(ndotv, roughness);
            }
        }

        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(pixels.size()) * sizeof(RgFloat);
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(imageBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
        void* mapped = nullptr;
        require(vkMapMemory(device_, stagingMemory, 0, imageBytes, 0, &mapped), "vkMapMemory(brdf staging)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(imageBytes));
        vkUnmapMemory(device_, stagingMemory);

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.extent = {size, size, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.format = VK_FORMAT_R32G32_SFLOAT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateImage(device_, &image, nullptr, &texture.image), "vkCreateImage(environment brdf)");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = findMemoryType(physicalDevice_, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(vkAllocateMemory(device_, &allocate, nullptr, &texture.memory), "vkAllocateMemory(environment brdf)");
        require(vkBindImageMemory(device_, texture.image, texture.memory, 0), "vkBindImageMemory(environment brdf)");

        VkCommandBuffer commandBuffer = beginOneTimeCommands();
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {size, size, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        transitionImage(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        endOneTimeCommands(commandBuffer);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        texture.mipLevels = 1;
        texture.view = createImageView(texture.image, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureResources() {
        const std::size_t materialCount = std::max<std::size_t>(1, geometry_.materials.size());
        materialTextures_.resize(materialCount);
        for (std::size_t i = 0; i < materialTextures_.size(); ++i) {
            const GpuPreviewGeometry::MaterialTextures material = i < geometry_.materials.size()
                ? geometry_.materials[i]
                : GpuPreviewGeometry::MaterialTextures{};
            createSampledTexture(material.albedoTexturePath, {255, 255, 255, 255}, materialTextures_[i].textures[0]);
            createSampledTexture(material.normalTexturePath, {128, 128, 255, 255}, materialTextures_[i].textures[1]);
            createSampledTexture(material.roughnessTexturePath, {178, 178, 178, 255}, materialTextures_[i].textures[2]);
            createSampledTexture(material.displacementTexturePath, {128, 128, 128, 255}, materialTextures_[i].textures[3]);
        }

        createRgbeCubeTexture(geometry_.environmentBackgroundTexturePath, {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentBackgroundTexture]);
        createRgbeCubeTexture(geometry_.environmentDiffuseTexturePath, {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentDiffuseTexture]);
        for (std::uint32_t i = 0; i < kEnvironmentSpecularTextureCount; ++i) {
            createRgbeCubeTexture(geometry_.environmentSpecularTexturePaths[i], {0.22f, 0.34f, 0.55f, 1.0f}, sharedTextures_[kEnvironmentSpecularTextureBase + i]);
        }
        createEnvironmentBrdfTexture(sharedTextures_[kEnvironmentBrdfTextureIndex]);
        maxTextureMipLevels_ = 1;
        for (const MaterialTextureResources& material : materialTextures_) {
            for (const TextureResource& texture : material.textures) {
                maxTextureMipLevels_ = std::max(maxTextureMipLevels_, texture.mipLevels);
            }
        }
        for (const TextureResource& texture : sharedTextures_) {
            maxTextureMipLevels_ = std::max(maxTextureMipLevels_, texture.mipLevels);
        }

        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.minLod = 0.0f;
        sampler.maxLod = static_cast<float>(maxTextureMipLevels_ - 1);
        sampler.anisotropyEnable = samplerAnisotropy_ ? VK_TRUE : VK_FALSE;
        sampler.maxAnisotropy = maxSamplerAnisotropy_;
        require(vkCreateSampler(device_, &sampler, nullptr, &textureSampler_), "vkCreateSampler(texture)");
    }

    void createDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, kMaterialTextureCount + kSharedTextureCount + 3> bindings{};
        bindings[0] = cameraBinding;
        for (std::uint32_t i = 0; i < kMaterialTextureCount; ++i) {
            bindings[1 + i].binding = 1 + i;
            bindings[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[1 + i].descriptorCount = 1;
            bindings[1 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 5;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[5] = samplerBinding;
        for (std::uint32_t i = 0; i < kSharedTextureCount; ++i) {
            bindings[6 + i].binding = 6 + i;
            bindings[6 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[6 + i].descriptorCount = 1;
            bindings[6 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].binding = kDirectionalShadowTextureBinding;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].descriptorCount = 1;
        bindings[kMaterialTextureCount + kSharedTextureCount + 2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &descriptorSetLayout_), "vkCreateDescriptorSetLayout");

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<std::uint32_t>(materialTextures_.size());
        poolSizes[1].descriptorCount = static_cast<std::uint32_t>((kMaterialTextureCount + kSharedTextureCount + 1u) * materialTextures_.size());
        poolSizes[2].descriptorCount = static_cast<std::uint32_t>(materialTextures_.size());
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = static_cast<std::uint32_t>(materialTextures_.size());
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptorPool_), "vkCreateDescriptorPool");

        std::vector<VkDescriptorSetLayout> layouts(materialTextures_.size(), descriptorSetLayout_);
        descriptorSets_.resize(materialTextures_.size());
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = descriptorPool_;
        allocate.descriptorSetCount = static_cast<std::uint32_t>(descriptorSets_.size());
        allocate.pSetLayouts = layouts.data();
        require(vkAllocateDescriptorSets(device_, &allocate, descriptorSets_.data()), "vkAllocateDescriptorSets");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;

        std::vector<std::array<VkDescriptorImageInfo, kMaterialTextureCount + kSharedTextureCount + 1>> imageInfos(descriptorSets_.size());
        std::vector<std::array<VkWriteDescriptorSet, kMaterialTextureCount + kSharedTextureCount + 3>> writes(descriptorSets_.size());
        for (std::size_t setIndex = 0; setIndex < descriptorSets_.size(); ++setIndex) {
            std::array<VkWriteDescriptorSet, kMaterialTextureCount + kSharedTextureCount + 3>& setWrites = writes[setIndex];
            setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[0].dstSet = descriptorSets_[setIndex];
            setWrites[0].dstBinding = 0;
            setWrites[0].descriptorCount = 1;
            setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            setWrites[0].pBufferInfo = &bufferInfo;

            for (std::uint32_t i = 0; i < kMaterialTextureCount; ++i) {
                imageInfos[setIndex][i].imageView = materialTextures_[setIndex].textures[i].view;
                imageInfos[setIndex][i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                setWrites[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                setWrites[1 + i].dstSet = descriptorSets_[setIndex];
                setWrites[1 + i].dstBinding = 1 + i;
                setWrites[1 + i].descriptorCount = 1;
                setWrites[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                setWrites[1 + i].pImageInfo = &imageInfos[setIndex][i];
            }

            setWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[5].dstSet = descriptorSets_[setIndex];
            setWrites[5].dstBinding = 5;
            setWrites[5].descriptorCount = 1;
            setWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            setWrites[5].pImageInfo = &samplerInfo;

            for (std::uint32_t i = 0; i < kSharedTextureCount; ++i) {
                const std::uint32_t infoIndex = kMaterialTextureCount + i;
                imageInfos[setIndex][infoIndex].imageView = sharedTextures_[i].view;
                imageInfos[setIndex][infoIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                setWrites[6 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                setWrites[6 + i].dstSet = descriptorSets_[setIndex];
                setWrites[6 + i].dstBinding = 6 + i;
                setWrites[6 + i].descriptorCount = 1;
                setWrites[6 + i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                setWrites[6 + i].pImageInfo = &imageInfos[setIndex][infoIndex];
            }
            const std::uint32_t shadowInfoIndex = kMaterialTextureCount + kSharedTextureCount;
            imageInfos[setIndex][shadowInfoIndex].imageView = shadowDepthView_;
            imageInfos[setIndex][shadowInfoIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].dstSet = descriptorSets_[setIndex];
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].dstBinding = kDirectionalShadowTextureBinding;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].descriptorCount = 1;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            setWrites[kMaterialTextureCount + kSharedTextureCount + 2].pImageInfo = &imageInfos[setIndex][shadowInfoIndex];
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
        }
    }

    void createV4ComposeDescriptors() {
        VkDescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.binding = 0;
        cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraBinding.descriptorCount = 1;
        cameraBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 5;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding albedoBinding{};
        albedoBinding.binding = kGBufferAlbedoBinding;
        albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        albedoBinding.descriptorCount = 1;
        albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding normalBinding = albedoBinding;
        normalBinding.binding = kGBufferNormalBinding;
        VkDescriptorSetLayoutBinding worldBinding = albedoBinding;
        worldBinding.binding = kGBufferWorldBinding;
        VkDescriptorSetLayoutBinding ssaoBinding = albedoBinding;
        ssaoBinding.binding = kSsaoRawBinding;
        VkDescriptorSetLayoutBinding ssaoBlurBinding = albedoBinding;
        ssaoBlurBinding.binding = kSsaoBlurBinding;
        VkDescriptorSetLayoutBinding manyLightBinding{};
        manyLightBinding.binding = kV4ManyLightBufferBinding;
        manyLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        manyLightBinding.descriptorCount = 1;
        manyLightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 8> bindings{
            cameraBinding,
            samplerBinding,
            albedoBinding,
            normalBinding,
            worldBinding,
            ssaoBinding,
            ssaoBlurBinding,
            manyLightBinding,
        };
        VkDescriptorSetLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout.pBindings = bindings.data();
        require(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &v4DescriptorSetLayout_), "vkCreateDescriptorSetLayout(v4)");

        std::array<VkDescriptorPoolSize, 4> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = 1;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[2].descriptorCount = 5;
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[3].descriptorCount = 1;
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        pool.pPoolSizes = poolSizes.data();
        pool.maxSets = 1;
        require(vkCreateDescriptorPool(device_, &pool, nullptr, &v4DescriptorPool_), "vkCreateDescriptorPool(v4)");

        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = v4DescriptorPool_;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &v4DescriptorSetLayout_;
        require(vkAllocateDescriptorSets(device_, &allocate, &v4DescriptorSet_), "vkAllocateDescriptorSets(v4)");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraUniform);
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = textureSampler_;
        VkDescriptorImageInfo albedoInfo{};
        albedoInfo.imageView = gbufferTargets_[0].view;
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageView = gbufferTargets_[1].view;
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo worldInfo{};
        worldInfo.imageView = gbufferTargets_[2].view;
        worldInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo ssaoInfo{};
        ssaoInfo.imageView = ssaoTarget_.view;
        ssaoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo ssaoBlurInfo{};
        ssaoBlurInfo.imageView = ssaoBlurTarget_.view;
        ssaoBlurInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorBufferInfo manyLightInfo{};
        manyLightInfo.buffer = lightBuffer_;
        manyLightInfo.offset = 0;
        manyLightInfo.range = lightBytes_;

        std::array<VkWriteDescriptorSet, 8> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = v4DescriptorSet_;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &bufferInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = v4DescriptorSet_;
        writes[1].dstBinding = 5;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[1].pImageInfo = &samplerInfo;
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = v4DescriptorSet_;
        writes[2].dstBinding = kGBufferAlbedoBinding;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[2].pImageInfo = &albedoInfo;
        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = v4DescriptorSet_;
        writes[3].dstBinding = kGBufferNormalBinding;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[3].pImageInfo = &normalInfo;
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = v4DescriptorSet_;
        writes[4].dstBinding = kGBufferWorldBinding;
        writes[4].descriptorCount = 1;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[4].pImageInfo = &worldInfo;
        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = v4DescriptorSet_;
        writes[5].dstBinding = kSsaoRawBinding;
        writes[5].descriptorCount = 1;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[5].pImageInfo = &ssaoInfo;
        writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[6].dstSet = v4DescriptorSet_;
        writes[6].dstBinding = kSsaoBlurBinding;
        writes[6].descriptorCount = 1;
        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[6].pImageInfo = &ssaoBlurInfo;
        writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[7].dstSet = v4DescriptorSet_;
        writes[7].dstBinding = kV4ManyLightBufferBinding;
        writes[7].descriptorCount = 1;
        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[7].pBufferInfo = &manyLightInfo;
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    VkShaderModule createShader(const std::filesystem::path& path) {
        const std::vector<std::uint32_t> code = readSpirv(resolveProjectPath(path));
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(std::uint32_t);
        createInfo.pCode = code.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        require(vkCreateShaderModule(device_, &createInfo, nullptr, &shader), "vkCreateShaderModule");
        return shader;
    }

    void createPipeline() {
        const VkShaderModule vertexShader = createShader("shaders/vulkan_gpu/simple_color.vert.spv");
        const VkShaderModule fragmentShader = createShader("shaders/vulkan_gpu/simple_color.frag.spv");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentShader;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(GpuPreviewVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        std::array<VkVertexInputAttributeDescription, 9> attributes{};
        attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, px)};
        attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, nx)};
        attributes[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, r)};
        attributes[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GpuPreviewVertex, u)};
        attributes[4] = {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, textured)};
        attributes[5] = {5, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, roughness)};
        attributes[6] = {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, metalness)};
        attributes[7] = {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, kind)};
        attributes[8] = {8, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewVertex, tx)};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = msaaSamples_;

        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlend{};
        colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &colorBlend;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(std::size(dynamicStates));
        dynamic.pDynamicStates = dynamicStates;

        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptorSetLayout_;
        require(vkCreatePipelineLayout(device_, &layout, nullptr, &pipelineLayout_), "vkCreatePipelineLayout");

        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.stageCount = 2;
        createInfo.pStages = stages;
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pViewportState = &viewportState;
        createInfo.pRasterizationState = &raster;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depth;
        createInfo.pColorBlendState = &blend;
        createInfo.pDynamicState = &dynamic;
        createInfo.layout = pipelineLayout_;
        createInfo.renderPass = renderPass_;
        createInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline_), "vkCreateGraphicsPipelines");

        const VkShaderModule shadowVertexShader = createShader("shaders/vulkan_gpu/shadow_depth.vert.spv");
        VkPipelineShaderStageCreateInfo shadowStage{};
        shadowStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shadowStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shadowStage.module = shadowVertexShader;
        shadowStage.pName = "main";

        VkPipelineRasterizationStateCreateInfo shadowRaster{};
        shadowRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        shadowRaster.polygonMode = VK_POLYGON_MODE_FILL;
        shadowRaster.cullMode = VK_CULL_MODE_BACK_BIT;
        shadowRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        shadowRaster.depthBiasEnable = VK_TRUE;
        shadowRaster.depthBiasConstantFactor = 1.4f;
        shadowRaster.depthBiasSlopeFactor = 1.8f;
        shadowRaster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo shadowMultisample{};
        shadowMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        shadowMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendStateCreateInfo shadowBlend{};
        shadowBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        VkGraphicsPipelineCreateInfo shadowCreateInfo{};
        shadowCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        shadowCreateInfo.stageCount = 1;
        shadowCreateInfo.pStages = &shadowStage;
        shadowCreateInfo.pVertexInputState = &vertexInput;
        shadowCreateInfo.pInputAssemblyState = &inputAssembly;
        shadowCreateInfo.pViewportState = &viewportState;
        shadowCreateInfo.pRasterizationState = &shadowRaster;
        shadowCreateInfo.pMultisampleState = &shadowMultisample;
        shadowCreateInfo.pDepthStencilState = &depth;
        shadowCreateInfo.pColorBlendState = &shadowBlend;
        shadowCreateInfo.pDynamicState = &dynamic;
        shadowCreateInfo.layout = pipelineLayout_;
        shadowCreateInfo.renderPass = shadowRenderPass_;
        shadowCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &shadowCreateInfo, nullptr, &shadowPipeline_), "vkCreateGraphicsPipelines(shadow)");
        vkDestroyShaderModule(device_, shadowVertexShader, nullptr);

        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);

        const VkShaderModule skyVertexShader = createShader("shaders/vulkan_gpu/skybox.vert.spv");
        const VkShaderModule skyFragmentShader = createShader("shaders/vulkan_gpu/skybox.frag.spv");
        VkPipelineShaderStageCreateInfo skyStages[2]{};
        skyStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        skyStages[0].module = skyVertexShader;
        skyStages[0].pName = "main";
        skyStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        skyStages[1].module = skyFragmentShader;
        skyStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo skyVertexInput{};
        skyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineRasterizationStateCreateInfo skyRaster{};
        skyRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        skyRaster.polygonMode = VK_POLYGON_MODE_FILL;
        skyRaster.cullMode = VK_CULL_MODE_NONE;
        skyRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        skyRaster.lineWidth = 1.0f;
        VkPipelineDepthStencilStateCreateInfo skyDepth{};
        skyDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        skyDepth.depthTestEnable = VK_FALSE;
        skyDepth.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo skyCreateInfo{};
        skyCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        skyCreateInfo.stageCount = 2;
        skyCreateInfo.pStages = skyStages;
        skyCreateInfo.pVertexInputState = &skyVertexInput;
        skyCreateInfo.pInputAssemblyState = &inputAssembly;
        skyCreateInfo.pViewportState = &viewportState;
        skyCreateInfo.pRasterizationState = &skyRaster;
        skyCreateInfo.pMultisampleState = &multisample;
        skyCreateInfo.pDepthStencilState = &skyDepth;
        skyCreateInfo.pColorBlendState = &blend;
        skyCreateInfo.pDynamicState = &dynamic;
        skyCreateInfo.layout = pipelineLayout_;
        skyCreateInfo.renderPass = renderPass_;
        skyCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &skyCreateInfo, nullptr, &skyPipeline_), "vkCreateGraphicsPipelines(sky)");

        if (enableV4Ssao_) {
            const VkShaderModule gbufferVertexShader = createShader("shaders/vulkan_gpu/simple_color.vert.spv");
            const VkShaderModule gbufferFragmentShader = createShader("shaders/vulkan_gpu/v4_gbuffer.frag.spv");
            VkPipelineShaderStageCreateInfo gbufferStages[2]{};
            gbufferStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            gbufferStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            gbufferStages[0].module = gbufferVertexShader;
            gbufferStages[0].pName = "main";
            gbufferStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            gbufferStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            gbufferStages[1].module = gbufferFragmentShader;
            gbufferStages[1].pName = "main";

            VkPipelineMultisampleStateCreateInfo gbufferMultisample{};
            gbufferMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            gbufferMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            std::array<VkPipelineColorBlendAttachmentState, 3> gbufferColorBlends{};
            for (VkPipelineColorBlendAttachmentState& attachment : gbufferColorBlends) {
                attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            }
            VkPipelineColorBlendStateCreateInfo gbufferBlend{};
            gbufferBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            gbufferBlend.attachmentCount = static_cast<std::uint32_t>(gbufferColorBlends.size());
            gbufferBlend.pAttachments = gbufferColorBlends.data();

            VkGraphicsPipelineCreateInfo gbufferCreateInfo{};
            gbufferCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            gbufferCreateInfo.stageCount = 2;
            gbufferCreateInfo.pStages = gbufferStages;
            gbufferCreateInfo.pVertexInputState = &vertexInput;
            gbufferCreateInfo.pInputAssemblyState = &inputAssembly;
            gbufferCreateInfo.pViewportState = &viewportState;
            gbufferCreateInfo.pRasterizationState = &raster;
            gbufferCreateInfo.pMultisampleState = &gbufferMultisample;
            gbufferCreateInfo.pDepthStencilState = &depth;
            gbufferCreateInfo.pColorBlendState = &gbufferBlend;
            gbufferCreateInfo.pDynamicState = &dynamic;
            gbufferCreateInfo.layout = pipelineLayout_;
            gbufferCreateInfo.renderPass = gbufferRenderPass_;
            gbufferCreateInfo.subpass = 0;
            require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gbufferCreateInfo, nullptr, &gbufferPipeline_), "vkCreateGraphicsPipelines(gbuffer)");
            vkDestroyShaderModule(device_, gbufferFragmentShader, nullptr);
            vkDestroyShaderModule(device_, gbufferVertexShader, nullptr);

            const VkShaderModule instancedVertexShader = createShader("shaders/vulkan_gpu/v4_instanced_sphere.vert.spv");
            const VkShaderModule instancedFragmentShader = createShader("shaders/vulkan_gpu/v4_gbuffer.frag.spv");
            VkPipelineShaderStageCreateInfo instancedStages[2]{};
            instancedStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            instancedStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            instancedStages[0].module = instancedVertexShader;
            instancedStages[0].pName = "main";
            instancedStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            instancedStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            instancedStages[1].module = instancedFragmentShader;
            instancedStages[1].pName = "main";

            std::array<VkVertexInputBindingDescription, 2> instancedBindings{};
            instancedBindings[0] = binding;
            instancedBindings[1].binding = 1;
            instancedBindings[1].stride = sizeof(GpuPreviewSphereInstance);
            instancedBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            std::array<VkVertexInputAttributeDescription, 12> instancedAttributes{};
            std::copy(attributes.begin(), attributes.end(), instancedAttributes.begin());
            instancedAttributes[9] = {9, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewSphereInstance, px)};
            instancedAttributes[10] = {10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewSphereInstance, r)};
            instancedAttributes[11] = {11, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(GpuPreviewSphereInstance, metalness)};

            VkPipelineVertexInputStateCreateInfo instancedVertexInput{};
            instancedVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            instancedVertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(instancedBindings.size());
            instancedVertexInput.pVertexBindingDescriptions = instancedBindings.data();
            instancedVertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(instancedAttributes.size());
            instancedVertexInput.pVertexAttributeDescriptions = instancedAttributes.data();

            VkGraphicsPipelineCreateInfo instancedCreateInfo = gbufferCreateInfo;
            instancedCreateInfo.pStages = instancedStages;
            instancedCreateInfo.pVertexInputState = &instancedVertexInput;
            require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &instancedCreateInfo, nullptr, &instancedGBufferPipeline_), "vkCreateGraphicsPipelines(gbuffer instanced)");
            vkDestroyShaderModule(device_, instancedFragmentShader, nullptr);
            vkDestroyShaderModule(device_, instancedVertexShader, nullptr);

            VkPipelineLayoutCreateInfo composeLayout{};
            composeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            composeLayout.setLayoutCount = 1;
            composeLayout.pSetLayouts = &v4DescriptorSetLayout_;
            require(vkCreatePipelineLayout(device_, &composeLayout, nullptr, &v4ComposePipelineLayout_), "vkCreatePipelineLayout(v4 compose)");

            const VkShaderModule fullscreenVertexShader = createShader("shaders/vulkan_gpu/v4_fullscreen.vert.spv");
            const VkShaderModule ssaoFragmentShader = createShader("shaders/vulkan_gpu/v4_ssao.frag.spv");
            const VkShaderModule ssaoBlurFragmentShader = createShader("shaders/vulkan_gpu/v4_ssao_blur.frag.spv");
            const VkShaderModule composeFragmentShader = createShader("shaders/vulkan_gpu/v4_ssao_compose.frag.spv");
            VkPipelineDepthStencilStateCreateInfo composeDepth{};
            composeDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            composeDepth.depthTestEnable = VK_FALSE;
            composeDepth.depthWriteEnable = VK_FALSE;
            VkPipelineRasterizationStateCreateInfo composeRaster = skyRaster;
            composeRaster.cullMode = VK_CULL_MODE_NONE;
            VkPipelineMultisampleStateCreateInfo fullscreenMultisample{};
            fullscreenMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            fullscreenMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            auto createFullscreenPipeline = [&](VkShaderModule fragmentShader, VkRenderPass targetRenderPass, VkSampleCountFlagBits samples, const char* label) {
                VkPipelineShaderStageCreateInfo stages[2]{};
                stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
                stages[0].module = fullscreenVertexShader;
                stages[0].pName = "main";
                stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                stages[1].module = fragmentShader;
                stages[1].pName = "main";
                VkPipelineMultisampleStateCreateInfo passMultisample = fullscreenMultisample;
                passMultisample.rasterizationSamples = samples;
                VkGraphicsPipelineCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                createInfo.stageCount = 2;
                createInfo.pStages = stages;
                createInfo.pVertexInputState = &skyVertexInput;
                createInfo.pInputAssemblyState = &inputAssembly;
                createInfo.pViewportState = &viewportState;
                createInfo.pRasterizationState = &composeRaster;
                createInfo.pMultisampleState = &passMultisample;
                createInfo.pDepthStencilState = &composeDepth;
                createInfo.pColorBlendState = &blend;
                createInfo.pDynamicState = &dynamic;
                createInfo.layout = v4ComposePipelineLayout_;
                createInfo.renderPass = targetRenderPass;
                createInfo.subpass = 0;
                VkPipeline created = VK_NULL_HANDLE;
                require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &createInfo, nullptr, &created), label);
                return created;
            };

            ssaoPipeline_ = createFullscreenPipeline(ssaoFragmentShader, ssaoRenderPass_, VK_SAMPLE_COUNT_1_BIT, "vkCreateGraphicsPipelines(v4 ssao)");
            ssaoBlurPipeline_ = createFullscreenPipeline(ssaoBlurFragmentShader, ssaoRenderPass_, VK_SAMPLE_COUNT_1_BIT, "vkCreateGraphicsPipelines(v4 ssao blur)");
            v4ComposePipeline_ = createFullscreenPipeline(composeFragmentShader, renderPass_, msaaSamples_, "vkCreateGraphicsPipelines(v4 compose)");
            vkDestroyShaderModule(device_, composeFragmentShader, nullptr);
            vkDestroyShaderModule(device_, ssaoBlurFragmentShader, nullptr);
            vkDestroyShaderModule(device_, ssaoFragmentShader, nullptr);
            vkDestroyShaderModule(device_, fullscreenVertexShader, nullptr);
        }

        vkDestroyShaderModule(device_, skyFragmentShader, nullptr);
        vkDestroyShaderModule(device_, skyVertexShader, nullptr);
    }

    void createCommands() {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = queueFamilies_.graphics;
        require(vkCreateCommandPool(device_, &pool, nullptr, &commandPool_), "vkCreateCommandPool");

        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = commandPool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        require(vkAllocateCommandBuffers(device_, &allocate, &commandBuffer_), "vkAllocateCommandBuffers");
    }

    void createSync() {
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &imageAvailable_), "vkCreateSemaphore");
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &renderFinished_), "vkCreateSemaphore");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        require(vkCreateFence(device_, &fence, nullptr, &inFlight_), "vkCreateFence");
    }

    void updateUniform(const V1CameraSettings& cameraSettings, std::uint32_t v4DebugMode) {
        const Vec3 eye = eyeOf(cameraSettings);
        const Vec3 forward = normalize(targetOf(cameraSettings) - eye);
        const Vec3 right = rightFor(forward);
        const Vec3 up = normalize(cross(right, forward));
        const float nearPlane = cameraSettings.nearPlane > 0.0f ? cameraSettings.nearPlane : 0.05f;
        const float farPlane = cameraSettings.farPlane > 0.0f ? cameraSettings.farPlane : 200.0f;
        const float fovY = cameraSettings.fovY > 0.0f ? cameraSettings.fovY : (52.0f * kPi / 180.0f);
        const float aspect = static_cast<float>(swapchainExtent_.width) / static_cast<float>(swapchainExtent_.height);

        CameraUniform uniform{};
        uniform.eyeNear[0] = eye.x;
        uniform.eyeNear[1] = eye.y;
        uniform.eyeNear[2] = eye.z;
        uniform.eyeNear[3] = nearPlane;
        uniform.rightFar[0] = right.x;
        uniform.rightFar[1] = right.y;
        uniform.rightFar[2] = right.z;
        uniform.rightFar[3] = farPlane;
        uniform.upTanHalf[0] = up.x;
        uniform.upTanHalf[1] = up.y;
        uniform.upTanHalf[2] = up.z;
        uniform.upTanHalf[3] = std::tan(fovY * 0.5f);
        uniform.forwardAspect[0] = forward.x;
        uniform.forwardAspect[1] = forward.y;
        uniform.forwardAspect[2] = forward.z;
        uniform.forwardAspect[3] = aspect;

        const Vec3 sunToScene = normalize({0.28f, 0.44f, -0.85f});
        Vec3 shadowRight = normalize(cross({0.0f, 0.0f, 1.0f}, sunToScene));
        if (dot(shadowRight, shadowRight) <= 0.00001f) {
            shadowRight = {1.0f, 0.0f, 0.0f};
        }
        const Vec3 shadowUp = normalize(cross(sunToScene, shadowRight));
        const Vec3 shadowCenter{0.0f, 0.0f, 0.65f};
        uniform.shadowRightExtent[0] = shadowRight.x;
        uniform.shadowRightExtent[1] = shadowRight.y;
        uniform.shadowRightExtent[2] = shadowRight.z;
        uniform.shadowRightExtent[3] = 5.6f;
        uniform.shadowUpNear[0] = shadowUp.x;
        uniform.shadowUpNear[1] = shadowUp.y;
        uniform.shadowUpNear[2] = shadowUp.z;
        uniform.shadowUpNear[3] = -4.8f;
        uniform.shadowForwardFar[0] = sunToScene.x;
        uniform.shadowForwardFar[1] = sunToScene.y;
        uniform.shadowForwardFar[2] = sunToScene.z;
        uniform.shadowForwardFar[3] = 6.8f;
        uniform.shadowCenterBias[0] = shadowCenter.x;
        uniform.shadowCenterBias[1] = shadowCenter.y;
        uniform.shadowCenterBias[2] = shadowCenter.z;
        uniform.shadowCenterBias[3] = 0.0025f;
        uniform.pointPosRadius[0] = 2.8f;
        uniform.pointPosRadius[1] = -1.8f;
        uniform.pointPosRadius[2] = 2.6f;
        uniform.pointPosRadius[3] = 4.8f;
        uniform.pointColorIntensity[0] = 1.0f;
        uniform.pointColorIntensity[1] = 0.62f;
        uniform.pointColorIntensity[2] = 0.30f;
        uniform.pointColorIntensity[3] = 1.25f;
        const Vec3 spotDir = normalize({0.18f, 0.72f, -0.67f});
        uniform.spotPosInner[0] = -0.6f;
        uniform.spotPosInner[1] = -4.4f;
        uniform.spotPosInner[2] = 4.8f;
        uniform.spotPosInner[3] = std::cos(20.0f * kPi / 180.0f);
        uniform.spotDirOuter[0] = spotDir.x;
        uniform.spotDirOuter[1] = spotDir.y;
        uniform.spotDirOuter[2] = spotDir.z;
        uniform.spotDirOuter[3] = std::cos(42.0f * kPi / 180.0f);
        uniform.spotColorIntensity[0] = 0.45f;
        uniform.spotColorIntensity[1] = 0.68f;
        uniform.spotColorIntensity[2] = 1.0f;
        uniform.spotColorIntensity[3] = 0.85f;
        uniform.v3Flags[0] = enableV4Ssao_ ? 2.0f : (enableV3Shadows_ ? 1.0f : 0.0f);
        uniform.v3Flags[1] = 2.4f;
        uniform.v3Flags[2] = 4.5f;
        uniform.v3Flags[3] = 7.2f;
        uniform.v4Flags[0] = geometry_.manyLightDemo ? 1.0f : 0.0f;
        uniform.v4Flags[1] = static_cast<float>(lightCount_);
        uniform.v4Flags[2] = static_cast<float>(v4DebugMode);
        uniform.v4Flags[3] = 1.0f;

        void* mapped = nullptr;
        require(vkMapMemory(device_, uniformMemory_, 0, sizeof(uniform), 0, &mapped), "vkMapMemory(uniform)");
        std::memcpy(mapped, &uniform, sizeof(uniform));
        vkUnmapMemory(device_, uniformMemory_);
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer");

        if (enableV3Shadows_) {
            VkClearValue shadowClear{};
            shadowClear.depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo shadowPass{};
            shadowPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPass.renderPass = shadowRenderPass_;
            shadowPass.framebuffer = shadowFramebuffer_;
            shadowPass.renderArea.extent = {kShadowAtlasWidth, kShadowAtlasHeight};
            shadowPass.clearValueCount = 1;
            shadowPass.pClearValues = &shadowClear;
            vkCmdBeginRenderPass(commandBuffer, &shadowPass, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
            const VkDescriptorSet shadowDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &shadowDescriptorSet, 0, nullptr);
            VkDeviceSize shadowOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &shadowOffset);
            for (std::uint32_t shadowIndex = 0; shadowIndex < kShadowMapCount; ++shadowIndex) {
                const std::uint32_t col = shadowIndex % kShadowAtlasColumns;
                const std::uint32_t row = shadowIndex / kShadowAtlasColumns;
                VkViewport shadowViewport{};
                shadowViewport.x = static_cast<float>(col * kShadowTileSize);
                shadowViewport.y = static_cast<float>(row * kShadowTileSize);
                shadowViewport.width = static_cast<float>(kShadowTileSize);
                shadowViewport.height = static_cast<float>(kShadowTileSize);
                shadowViewport.minDepth = 0.0f;
                shadowViewport.maxDepth = 1.0f;
                VkRect2D shadowScissor{};
                shadowScissor.offset = {static_cast<std::int32_t>(col * kShadowTileSize), static_cast<std::int32_t>(row * kShadowTileSize)};
                shadowScissor.extent = {kShadowTileSize, kShadowTileSize};
                vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
                vkCmdDraw(commandBuffer, vertexCount_, 1, 0, shadowIndex);
            }
            vkCmdEndRenderPass(commandBuffer);
        }

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.extent = swapchainExtent_;

        if (enableV4Ssao_) {
            std::array<VkClearValue, 4> gbufferClears{};
            gbufferClears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};
            gbufferClears[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            gbufferClears[3].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo gbufferPass{};
            gbufferPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            gbufferPass.renderPass = gbufferRenderPass_;
            gbufferPass.framebuffer = gbufferFramebuffer_;
            gbufferPass.renderArea.extent = swapchainExtent_;
            gbufferPass.clearValueCount = static_cast<std::uint32_t>(gbufferClears.size());
            gbufferPass.pClearValues = gbufferClears.data();
            vkCmdBeginRenderPass(commandBuffer, &gbufferPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline_);
            VkDeviceSize gbufferOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &gbufferOffset);
            const VkDescriptorSet fallbackDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
            if (geometry_.batches.empty()) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
                vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
            } else {
                for (const GpuPreviewGeometry::Batch& batch : geometry_.batches) {
                    if (batch.vertexCount == 0) {
                        continue;
                    }
                    const std::uint32_t materialIndex = batch.materialIndex < descriptorSets_.size() ? batch.materialIndex : 0u;
                    const VkDescriptorSet descriptorSet = descriptorSets_[materialIndex];
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
                    vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
                }
            }
            if (sphereVertexCount_ > 0 && sphereInstanceCount_ > 0) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedGBufferPipeline_);
                const VkDescriptorSet instancedDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &instancedDescriptorSet, 0, nullptr);
                std::array<VkBuffer, 2> sphereBuffers{sphereVertexBuffer_, sphereInstanceBuffer_};
                std::array<VkDeviceSize, 2> sphereOffsets{0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<std::uint32_t>(sphereBuffers.size()), sphereBuffers.data(), sphereOffsets.data());
                vkCmdDraw(commandBuffer, sphereVertexCount_, sphereInstanceCount_, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            VkClearValue ssaoClear{};
            ssaoClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
            VkRenderPassBeginInfo ssaoPass{};
            ssaoPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            ssaoPass.renderPass = ssaoRenderPass_;
            ssaoPass.framebuffer = ssaoFramebuffer_;
            ssaoPass.renderArea.extent = swapchainExtent_;
            ssaoPass.clearValueCount = 1;
            ssaoPass.pClearValues = &ssaoClear;
            vkCmdBeginRenderPass(commandBuffer, &ssaoPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            if (!geometry_.manyLightDemo) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline_);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            VkClearValue blurClear{};
            blurClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
            VkRenderPassBeginInfo blurPass{};
            blurPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            blurPass.renderPass = ssaoRenderPass_;
            blurPass.framebuffer = ssaoBlurFramebuffer_;
            blurPass.renderArea.extent = swapchainExtent_;
            blurPass.clearValueCount = 1;
            blurPass.pClearValues = &blurClear;
            vkCmdBeginRenderPass(commandBuffer, &blurPass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            if (!geometry_.manyLightDemo) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoBlurPipeline_);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);

            std::array<VkClearValue, 2> composeClears{};
            composeClears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
            composeClears[1].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo composePass{};
            composePass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            composePass.renderPass = renderPass_;
            composePass.framebuffer = framebuffers_[imageIndex];
            composePass.renderArea.extent = swapchainExtent_;
            composePass.clearValueCount = static_cast<std::uint32_t>(composeClears.size());
            composePass.pClearValues = composeClears.data();
            vkCmdBeginRenderPass(commandBuffer, &composePass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v4ComposePipelineLayout_, 0, 1, &v4DescriptorSet_, 0, nullptr);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffer);
            require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
            return;
        }

        std::array<VkClearValue, 2> clears{};
        clears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPass{};
        renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPass.renderPass = renderPass_;
        renderPass.framebuffer = framebuffers_[imageIndex];
        renderPass.renderArea.extent = swapchainExtent_;
        renderPass.clearValueCount = static_cast<std::uint32_t>(clears.size());
        renderPass.pClearValues = clears.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPass, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        const VkDescriptorSet fallbackDescriptorSet = descriptorSets_.empty() ? VK_NULL_HANDLE : descriptorSets_.front();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &offset);
        if (geometry_.batches.empty()) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &fallbackDescriptorSet, 0, nullptr);
            vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
        } else {
            for (const GpuPreviewGeometry::Batch& batch : geometry_.batches) {
                if (batch.vertexCount == 0) {
                    continue;
                }
                const std::uint32_t materialIndex = batch.materialIndex < descriptorSets_.size() ? batch.materialIndex : 0u;
                const VkDescriptorSet descriptorSet = descriptorSets_[materialIndex];
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet, 0, nullptr);
                vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
            }
        }
        vkCmdEndRenderPass(commandBuffer);
        require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
    }

    HWND hwnd_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    const GpuPreviewGeometry& geometry_;
    bool enableV3Shadows_ = false;
    std::string gpuName_;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    QueueFamilies queueFamilies_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass gbufferRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass ssaoRenderPass_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkImage msaaColorImage_ = VK_NULL_HANDLE;
    VkDeviceMemory msaaColorMemory_ = VK_NULL_HANDLE;
    VkImageView msaaColorView_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    VkImage shadowDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowDepthMemory_ = VK_NULL_HANDLE;
    VkImageView shadowDepthView_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
    std::array<TextureResource, 3> gbufferTargets_{};
    VkImage gbufferDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory gbufferDepthMemory_ = VK_NULL_HANDLE;
    VkImageView gbufferDepthView_ = VK_NULL_HANDLE;
    VkFramebuffer gbufferFramebuffer_ = VK_NULL_HANDLE;
    TextureResource ssaoTarget_{};
    TextureResource ssaoBlurTarget_{};
    VkFramebuffer ssaoFramebuffer_ = VK_NULL_HANDLE;
    VkFramebuffer ssaoBlurFramebuffer_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexBytes_ = 0;
    std::uint32_t vertexCount_ = 0;
    VkBuffer sphereVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sphereVertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize sphereVertexBytes_ = 0;
    std::uint32_t sphereVertexCount_ = 0;
    VkBuffer sphereInstanceBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sphereInstanceMemory_ = VK_NULL_HANDLE;
    VkDeviceSize sphereInstanceBytes_ = 0;
    std::uint32_t sphereInstanceCount_ = 0;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory_ = VK_NULL_HANDLE;
    VkBuffer lightBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory lightMemory_ = VK_NULL_HANDLE;
    VkDeviceSize lightBytes_ = 0;
    std::uint32_t lightCount_ = 0;
    std::vector<MaterialTextureResources> materialTextures_;
    std::array<TextureResource, kSharedTextureCount> sharedTextures_{};
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    bool samplerAnisotropy_ = false;
    float maxSamplerAnisotropy_ = 1.0f;
    std::uint32_t maxTextureMipLevels_ = 1;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkDescriptorSetLayout v4DescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool v4DescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet v4DescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout v4ComposePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline skyPipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline gbufferPipeline_ = VK_NULL_HANDLE;
    VkPipeline instancedGBufferPipeline_ = VK_NULL_HANDLE;
    VkPipeline ssaoPipeline_ = VK_NULL_HANDLE;
    VkPipeline ssaoBlurPipeline_ = VK_NULL_HANDLE;
    VkPipeline v4ComposePipeline_ = VK_NULL_HANDLE;
    VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;
    std::uint32_t frameIndex_ = 0;
    bool enableV4Ssao_ = false;
};

struct PreviewState {
    V1RenderSettings settings;
    GpuPreviewGeometry geometry;
    std::unique_ptr<VulkanGpuRenderer> renderer;
    std::array<bool, 256> keys{};
    V1CameraSettings camera;
    bool roaming = false;
    bool mouseLook = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint32_t v4DebugMode = 0;
    std::uint32_t frameIndex = 0;
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
};

PreviewState* stateFrom(HWND hwnd) {
    return reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void deriveAnglesFromCamera(PreviewState& state) {
    const Vec3 forward = normalize(targetOf(state.camera) - eyeOf(state.camera));
    state.yaw = std::atan2(forward.y, forward.x);
    state.pitch = std::asin(std::clamp(forward.z, -1.0f, 1.0f));
}

void writeCameraPose(PreviewState& state, Vec3 eye) {
    const Vec3 forward = forwardFor(state.yaw, state.pitch);
    const Vec3 right = rightFor(forward);
    const Vec3 up = normalize(cross(right, forward));
    state.camera.enabled = true;
    state.camera.eyeX = eye.x;
    state.camera.eyeY = eye.y;
    state.camera.eyeZ = eye.z;
    state.camera.targetX = eye.x + forward.x;
    state.camera.targetY = eye.y + forward.y;
    state.camera.targetZ = eye.z + forward.z;
    state.camera.upX = up.x;
    state.camera.upY = up.y;
    state.camera.upZ = up.z;
}

bool keyDown(const PreviewState& state, int key) {
    return key >= 0 && key < static_cast<int>(state.keys.size()) && state.keys[static_cast<std::size_t>(key)];
}

int mouseXFrom(LPARAM lParam) {
    return static_cast<int>(static_cast<short>(LOWORD(lParam)));
}

int mouseYFrom(LPARAM lParam) {
    return static_cast<int>(static_cast<short>(HIWORD(lParam)));
}

void rotateCamera(PreviewState& state, float deltaX, float deltaY) {
    const float mouseSensitivity = 0.0045f;
    state.yaw -= deltaX * mouseSensitivity;
    state.pitch = std::clamp(state.pitch - deltaY * mouseSensitivity, -1.45f, 1.45f);
    writeCameraPose(state, eyeOf(state.camera));
}

void updateCamera(PreviewState& state, float dt) {
    if (!state.roaming) {
        state.camera = state.geometry.camera;
        return;
    }
    const float lookSpeed = 1.85f;
    if (keyDown(state, VK_LEFT) || keyDown(state, 'J')) state.yaw += lookSpeed * dt;
    if (keyDown(state, VK_RIGHT) || keyDown(state, 'L')) state.yaw -= lookSpeed * dt;
    if (keyDown(state, VK_UP) || keyDown(state, 'I')) state.pitch = std::clamp(state.pitch + lookSpeed * dt, -1.45f, 1.45f);
    if (keyDown(state, VK_DOWN) || keyDown(state, 'K')) state.pitch = std::clamp(state.pitch - lookSpeed * dt, -1.45f, 1.45f);

    const Vec3 forward = forwardFor(state.yaw, state.pitch);
    const Vec3 right = rightFor(forward);
    const Vec3 up{0.0f, 0.0f, 1.0f};
    Vec3 movement{};
    if (keyDown(state, 'W')) movement = movement + forward;
    if (keyDown(state, 'S')) movement = movement - forward;
    if (keyDown(state, 'D')) movement = movement + right;
    if (keyDown(state, 'A')) movement = movement - right;
    if (keyDown(state, 'E') || keyDown(state, VK_SPACE)) movement = movement + up;
    if (keyDown(state, 'Q') || keyDown(state, VK_CONTROL)) movement = movement - up;
    const float speed = (GetKeyState(VK_SHIFT) & 0x8000) ? 20.0f : 7.0f;
    if (dot(movement, movement) > 0.00001f) {
        movement = normalize(movement) * (speed * dt);
    }
    writeCameraPose(state, eyeOf(state.camera) + movement);
}

bool shouldAnimateGeometry(const PreviewState& state) {
    return !state.settings.enableV2Shading && state.settings.scenePath.extension() == ".s72";
}

void advanceAnimatedGeometry(PreviewState& state) {
    if (!shouldAnimateGeometry(state)) {
        return;
    }
    state.settings.frameIndex = state.frameIndex++;
    state.geometry = buildGpuPreviewGeometry(state.settings);
    state.renderer->updateGeometry(state.geometry);
}

LRESULT CALLBACK vulkanPreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        return 0;
    case WM_TIMER:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (!state->renderer) {
                return 0;
            }
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - state->lastTick).count();
            state->lastTick = now;
            try {
                advanceAnimatedGeometry(*state);
                updateCamera(*state, std::max(0.001f, static_cast<float>(elapsedMs) / 1000.0f));
                state->renderer->draw(state->camera, state->v4DebugMode);
            } catch (const std::exception& error) {
                std::cerr << "Vulkan preview draw failed: " << error.what() << '\n';
                DestroyWindow(hwnd);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam < 256) {
            if (PreviewState* state = stateFrom(hwnd)) {
                state->keys[static_cast<std::size_t>(wParam)] = true;
                if (wParam == 'R' && !(lParam & (1 << 30))) {
                    state->roaming = !state->roaming;
                    if (state->roaming) {
                        state->camera = state->geometry.camera;
                        deriveAnglesFromCamera(*state);
                        writeCameraPose(*state, eyeOf(state->camera));
                        SetWindowTextW(hwnd, L"VulkanRender GPU preview - roaming camera");
                        std::cout << "GPU camera roaming enabled. WASD move, Q/E vertical, arrows/IJKL or right-drag look, Shift fast.\n";
                    } else {
                        state->mouseLook = false;
                        ReleaseCapture();
                        state->camera = state->geometry.camera;
                        SetWindowTextW(hwnd, L"VulkanRender GPU preview - press R for roaming");
                        std::cout << "GPU camera roaming disabled.\n";
                    }
                }
                if (wParam >= '0' && wParam <= '5' && !(lParam & (1 << 30))) {
                    state->v4DebugMode = static_cast<std::uint32_t>(wParam - '0');
                    const wchar_t* titles[] = {
                        L"VulkanRender GPU preview - v4 final",
                        L"VulkanRender GPU preview - v4 albedo",
                        L"VulkanRender GPU preview - v4 normal",
                        L"VulkanRender GPU preview - v4 depth",
                        L"VulkanRender GPU preview - v4 SSAO raw",
                        L"VulkanRender GPU preview - v4 SSAO blur",
                    };
                    SetWindowTextW(hwnd, titles[state->v4DebugMode]);
                    std::cout << "V4 debug view " << state->v4DebugMode << " selected. 0 final, 1 albedo, 2 normal, 3 depth, 4 SSAO raw, 5 SSAO blur.\n";
                }
            }
        }
        return 0;
    case WM_RBUTTONDOWN:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (state->roaming) {
                state->mouseLook = true;
                state->lastMouseX = mouseXFrom(lParam);
                state->lastMouseY = mouseYFrom(lParam);
                SetCapture(hwnd);
            }
        }
        return 0;
    case WM_RBUTTONUP:
        if (PreviewState* state = stateFrom(hwnd)) {
            state->mouseLook = false;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (PreviewState* state = stateFrom(hwnd)) {
            if (state->roaming && state->mouseLook) {
                const int x = mouseXFrom(lParam);
                const int y = mouseYFrom(lParam);
                rotateCamera(*state, static_cast<float>(x - state->lastMouseX), static_cast<float>(y - state->lastMouseY));
                state->lastMouseX = x;
                state->lastMouseY = y;
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256) {
            if (PreviewState* state = stateFrom(hwnd)) {
                state->keys[static_cast<std::size_t>(wParam)] = false;
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kWindowTimer);
        if (PreviewState* state = stateFrom(hwnd)) {
            state->mouseLook = false;
        }
        ReleaseCapture();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} // namespace

int runVulkanPreviewWindow(V1RenderSettings settings) {
    resetPreviewLog();
    previewLog("runVulkanPreviewWindow: begin");
    PreviewState state;
    state.settings = std::move(settings);
    previewLog("runVulkanPreviewWindow: build geometry");
    state.geometry = buildGpuPreviewGeometry(state.settings);
    previewLog("runVulkanPreviewWindow: geometry vertices=" + std::to_string(state.geometry.vertices.size()));
    state.camera = state.geometry.camera;
    deriveAnglesFromCamera(state);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"LZJUVulkanRenderGpuPreview";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = vulkanPreviewProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    RECT rect{0, 0, static_cast<LONG>(state.settings.width), static_cast<LONG>(state.settings.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"VulkanRender GPU preview - R roaming, RMB drag look",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        &state
    );
    if (!hwnd) {
        std::cerr << "Could not create Vulkan preview window.\n";
        previewLog("runVulkanPreviewWindow: CreateWindowExW failed");
        return 1;
    }

    try {
        previewLog("runVulkanPreviewWindow: create renderer");
        state.renderer = std::make_unique<VulkanGpuRenderer>(hwnd, state.settings.width, state.settings.height, state.geometry, state.settings.enableV3Shadows, state.settings.enableV4Ssao);
    } catch (const std::exception& error) {
        std::cerr << "Could not initialize Vulkan GPU preview: " << error.what() << '\n';
        previewLog(std::string("runVulkanPreviewWindow: renderer init failed: ") + error.what());
        DestroyWindow(hwnd);
        return 1;
    }
    previewLog("runVulkanPreviewWindow: timer start");
    SetTimer(hwnd, kWindowTimer, 16, nullptr);

    std::cout << "Opened Vulkan GPU preview window. vertices=" << state.geometry.vertices.size()
              << ". Press R for roaming, hold right mouse to look, Esc to exit.\n";

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    state.renderer.reset();
    return 0;
}

} // namespace vr
