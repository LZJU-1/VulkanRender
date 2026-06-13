#include "core/RendererApp.hpp"

#include "core/FeatureProfile.hpp"
#include "core/RenderGraph.hpp"
#include "platform/PreviewWindow.hpp"
#include "platform/VulkanPreviewWindow.hpp"
#include "render/SoftwareV1Renderer.hpp"
#include "rhi/RenderDevice.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace vr {

RendererApp::RendererApp(AppConfig config) : config_(std::move(config)) {}

int RendererApp::run() {
    const auto profile = findProfile(config_.profile);
    if (!profile) {
        throw std::runtime_error("Profile disappeared after command line validation.");
    }

    auto device = createRenderDevice(true, config_.enableValidation);
    const DeviceReport report = device->report();

    if (config_.listDevices) {
        report.print(std::cout);
    }

    const bool rtAvailable = report.hasRayTracingDevice();
    if (profile->requiresRayTracing && config_.requireRayTracing && !rtAvailable) {
        std::cerr << "Realtime ray tracing was required, but no device exposes the required Vulkan RT extensions.\n";
        return 2;
    }

    if (profile->requiresRayTracing && !config_.enableRayTracing) {
        std::cout << "Note: v5-rt selected without --enable-rt; graph is shown in planning mode.\n";
    }
    if (profile->requiresRayTracing && config_.enableRayTracing && !rtAvailable) {
        std::cout << "Note: realtime RT requested but unavailable; running graph in fallback mode.\n";
    }

    const RenderGraph graph = RenderGraph::build(*profile);
    graph.describe(std::cout);

    if (!config_.scenePath.empty()) {
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
            ? (settings.enableV2Shading ? "assets/third_party/s72_examples/materials.s72" : "assets/scenes/v1.scene")
            : config_.scenePath;
        settings.outputPath = config_.outputPath;

        const V1RenderStats stats = renderSoftwareV1(settings);
        std::cout
            << "Rendered " << profile->key << " image: " << stats.outputPath.string()
            << " objects=" << stats.objectCount
            << " visible=" << stats.visibleObjects
            << " triangles=" << stats.trianglesSubmitted
            << '\n';
    }

    if (config_.previewWindow) {
        if (profile->id != ProfileId::V1SceneForward && profile->id != ProfileId::V2PbrIbl) {
            std::cout << "Realtime preview is currently implemented for v1 and the v2 software validation path only.\n";
            return 0;
        }

        V1RenderSettings settings;
        settings.width = config_.width;
        settings.height = config_.height;
        settings.enableV2Shading = profile->id == ProfileId::V2PbrIbl;
        settings.scenePath = config_.scenePath.empty()
            ? (settings.enableV2Shading ? "assets/third_party/s72_examples/materials.s72" : "assets/scenes/v1.scene")
            : config_.scenePath;
        settings.outputPath = config_.outputPath;
        if (settings.scenePath.extension() == ".scene") {
            std::cout << "GPU preview needs mesh geometry; falling back to the legacy CPU preview for .scene files.\n";
            return runV1PreviewWindow(settings);
        }
        return runVulkanPreviewWindow(settings);
    }

    return 0;
}

} // namespace vr
