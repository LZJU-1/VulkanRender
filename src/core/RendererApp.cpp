#include "core/RendererApp.hpp"

#include "core/FeatureProfile.hpp"
#include "core/RenderGraph.hpp"
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

    return 0;
}

} // namespace vr
