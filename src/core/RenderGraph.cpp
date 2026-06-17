#include "core/RenderGraph.hpp"

#include <ostream>
#include <utility>

namespace vr {

RenderGraph::RenderGraph(FeatureProfile profile) : profile_(std::move(profile)) {}

RenderGraph RenderGraph::build(const FeatureProfile& profile) {
    RenderGraph graph(profile);
    graph.add("scene.upload", PassKind::Setup);
    graph.add("animation.update", PassKind::Setup);
    graph.add("visibility.frustum-cull", PassKind::Compute);

    switch (profile.id) {
    case ProfileId::V1SceneForward:
        graph.add("forward.simple-material", PassKind::Raster);
        graph.add("present.tonemap", PassKind::PostProcess);
        break;
    case ProfileId::V2PbrIbl:
        graph.add("environment.ibl-precompute", PassKind::Compute);
        graph.add("forward.pbr-material", PassKind::Raster);
        graph.add("skybox.draw", PassKind::Raster);
        graph.add("post.tone-map", PassKind::PostProcess);
        break;
    case ProfileId::V3LightsShadows:
        graph.add("shadow.spot-perspective", PassKind::Raster);
        graph.add("shadow.sphere-omni", PassKind::Raster);
        graph.add("shadow.sun-cascades", PassKind::Raster);
        graph.add("forward.lit-pbr", PassKind::Raster);
        graph.add("debug.cascade-visualization", PassKind::PostProcess, true);
        break;
    case ProfileId::V4DeferredSsao:
        graph.add("gbuffer.fill", PassKind::Raster);
        graph.add("ssao.generate", PassKind::Compute);
        graph.add("ssao.blur", PassKind::Compute);
        graph.add("deferred.light-compose", PassKind::Raster);
        graph.add("post.tone-map", PassKind::PostProcess);
        break;
    case ProfileId::V6HybridRealtimeRayTracing:
        graph.add("gbuffer.fill-reference-hybrid", PassKind::Raster);
        graph.add("rt.build-blas", PassKind::RayTracing);
        graph.add("rt.build-tlas", PassKind::RayTracing);
        graph.add("rt.reference-raytraced-shadows", PassKind::RayTracing);
        graph.add("rt.reference-raytraced-ambient-occlusion", PassKind::RayTracing);
        graph.add("rt.reference-raytraced-reflections", PassKind::RayTracing);
        graph.add("denoise.svgf-shadow-ao", PassKind::Compute);
        graph.add("denoise.temporal-reflection", PassKind::Compute);
        graph.add("post.taa-resolve-and-downsample", PassKind::PostProcess);
        break;
    }

    graph.add("frame.readback", PassKind::Readback, true);
    return graph;
}

bool RenderGraph::containsRayTracingPass() const {
    for (const auto& pass : passes_) {
        if (pass.kind == PassKind::RayTracing) {
            return true;
        }
    }
    return false;
}

void RenderGraph::describe(std::ostream& out) const {
    out << "Profile: " << profile_.displayName << '\n';
    for (std::size_t i = 0; i < passes_.size(); ++i) {
        const auto& pass = passes_[i];
        out << "  [" << i << "] " << pass.name << " (" << toString(pass.kind) << ')';
        if (pass.optional) {
            out << " optional";
        }
        out << '\n';
    }
}

void RenderGraph::add(std::string name, PassKind kind, bool optional) {
    passes_.push_back(RenderPassInfo{std::move(name), kind, optional});
}

std::string toString(PassKind kind) {
    switch (kind) {
    case PassKind::Setup: return "setup";
    case PassKind::Raster: return "raster";
    case PassKind::Compute: return "compute";
    case PassKind::RayTracing: return "ray-tracing";
    case PassKind::PostProcess: return "post";
    case PassKind::Readback: return "readback";
    }
    return "unknown";
}

} // namespace vr
