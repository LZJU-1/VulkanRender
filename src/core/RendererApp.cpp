#include "core/RendererApp.hpp"

#include "core/FeatureProfile.hpp"
#include "core/RenderGraph.hpp"
#include "core/ValidationPipeline.hpp"
#include "platform/PreviewWindow.hpp"
#include "platform/VulkanPreviewWindow.hpp"
#include "render/SoftwareV1Renderer.hpp"
#include "rhi/RenderDevice.hpp"

#include <iostream>
#include <filesystem>
#include <cmath>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace vr {
namespace {

std::filesystem::path executablePath() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer);
#else
    return {};
#endif
}

std::filesystem::path resolveInputPath(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || std::filesystem::exists(path)) {
        return path;
    }

    std::filesystem::path cursor = executablePath().parent_path();
    while (!cursor.empty()) {
        const std::filesystem::path candidate = cursor / path;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return path;
}

void applyCameraOverride(const AppConfig& config, V1RenderSettings& settings) {
    if (!config.camera.enabled) {
        return;
    }
    settings.camera.enabled = true;
    settings.camera.eyeX = config.camera.eyeX;
    settings.camera.eyeY = config.camera.eyeY;
    settings.camera.eyeZ = config.camera.eyeZ;
    settings.camera.targetX = config.camera.targetX;
    settings.camera.targetY = config.camera.targetY;
    settings.camera.targetZ = config.camera.targetZ;
    settings.camera.upX = 0.0f;
    settings.camera.upY = 0.0f;
    settings.camera.upZ = 1.0f;
    settings.camera.fovY = config.camera.fovYDegrees > 0.0f
        ? config.camera.fovYDegrees * 3.14159265358979323846f / 180.0f
        : 0.0f;
}

} // namespace

RendererApp::RendererApp(AppConfig config) : config_(std::move(config)) {}

int RendererApp::run() {
    const auto profile = findProfile(config_.profile);
    if (!profile) {
        throw std::runtime_error("Profile disappeared after command line validation.");
    }

    if (config_.printValidationPipeline) {
        std::optional<ProfileId> onlyProfile;
        if (!config_.validationPipelineProfile.empty() && config_.validationPipelineProfile != "all") {
            onlyProfile = findProfile(config_.validationPipelineProfile)->id;
        }
        printValidationPipelines(std::cout, onlyProfile);
        return 0;
    }

    auto device = createRenderDevice(true, config_.enableValidation);
    const DeviceReport report = device->report();

    if (config_.listDevices) {
        report.print(std::cout);
    }

    const bool rtAvailable = report.hasRayTracingDevice();
    if (profile->requiresRayTracing) {
        config_.enableRayTracing = true;
    }

    if (profile->requiresRayTracing && config_.requireRayTracing && !rtAvailable) {
        std::cerr << "Realtime ray tracing was required, but no device exposes the required Vulkan RT extensions.\n";
        return 2;
    }

    if (profile->requiresRayTracing && config_.enableRayTracing && !rtAvailable) {
        std::cout << "Note: realtime RT requested but unavailable; running graph in fallback mode.\n";
    }

    const RenderGraph graph = RenderGraph::build(*profile);
    graph.describe(std::cout);
    const std::filesystem::path defaultV2Scene = "assets/third_party/s72_examples/materials.s72";
    const std::filesystem::path defaultV3Scene = "assets/third_party/s72_examples/v3_shadow_demo.shadowdemo";
    const std::filesystem::path defaultV4Scene = "assets/third_party/s72_examples/v4_many_lights.manylights";

    if (!config_.scenePath.empty()) {
        config_.scenePath = resolveInputPath(config_.scenePath).string();
        std::cout << "Scene: " << config_.scenePath << '\n';
    }

    for (std::uint32_t frame = 0; frame < config_.frames; ++frame) {
        std::cout << "Frame " << frame << ":";
        for (const auto& pass : graph.passes()) {
            if (pass.kind == PassKind::RayTracing && (!config_.enableRayTracing || !rtAvailable)) {
                std::cout << " skip(" << pass.name << ')';
            } else {
                std::cout << ' ' << pass.name;
            }
        }
        std::cout << '\n';
    }

    if (config_.renderImage) {
        if (profile->id != ProfileId::V1SceneForward && profile->id != ProfileId::V2PbrIbl) {
            std::cout << "Render output is currently implemented for v1 and the v2 software validation path only; selected profile was not rendered.\n";
            return 0;
        }

        V1RenderSettings settings;
        settings.width = config_.width;
        settings.height = config_.height;
        settings.frameIndex = config_.frames - 1u;
        settings.enableV2Shading = profile->id == ProfileId::V2PbrIbl;
        settings.scenePath = config_.scenePath.empty()
            ? (settings.enableV2Shading ? defaultV2Scene : "assets/scenes/v1.scene")
            : config_.scenePath;
        settings.scenePath = resolveInputPath(settings.scenePath);
        settings.outputPath = config_.outputPath;
        applyCameraOverride(config_, settings);

        const V1RenderStats stats = renderSoftwareV1(settings);
        std::cout
            << "Rendered " << profile->key << " image: " << stats.outputPath.string()
            << " objects=" << stats.objectCount
            << " visible=" << stats.visibleObjects
            << " triangles=" << stats.trianglesSubmitted
            << '\n';
    }

    if (config_.previewWindow) {
        if (profile->id != ProfileId::V1SceneForward && profile->id != ProfileId::V2PbrIbl && profile->id != ProfileId::V3LightsShadows && profile->id != ProfileId::V4DeferredSsao && profile->id != ProfileId::V6HybridRealtimeRayTracing) {
            std::cout << "Realtime preview is currently implemented for v1, v2, v3, v4, and v6.\n";
            return 0;
        }

        V1RenderSettings settings;
        settings.width = config_.width;
        settings.height = config_.height;
        settings.enableV5RayTracing = profile->id == ProfileId::V6HybridRealtimeRayTracing;
        settings.enableV2Shading = profile->id == ProfileId::V2PbrIbl || profile->id == ProfileId::V3LightsShadows || profile->id == ProfileId::V4DeferredSsao || settings.enableV5RayTracing;
        settings.enableV3Shadows = profile->id == ProfileId::V3LightsShadows || profile->id == ProfileId::V4DeferredSsao;
        settings.enableV4Ssao = profile->id == ProfileId::V4DeferredSsao;
        settings.scenePath = config_.scenePath.empty()
            ? (profile->id == ProfileId::V4DeferredSsao ? defaultV4Scene : (profile->id == ProfileId::V3LightsShadows ? defaultV3Scene : (settings.enableV2Shading ? defaultV2Scene : "assets/scenes/v1.scene")))
            : config_.scenePath;
        settings.scenePath = resolveInputPath(settings.scenePath);
        settings.outputPath = config_.outputPath;
        applyCameraOverride(config_, settings);
        if (settings.scenePath.extension() == ".scene") {
            std::cout << "GPU preview needs mesh geometry; falling back to the legacy CPU preview for .scene files.\n";
            return runV1PreviewWindow(settings);
        }
        return runVulkanPreviewWindow(settings);
    }

    return 0;
}

} // namespace vr
