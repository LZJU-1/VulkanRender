#include "core/FeatureProfile.hpp"
#include "core/RenderGraph.hpp"
#include "core/ValidationPipeline.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "test failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    require(vr::featureProfiles().size() == 5, "expected five staged profiles");

    const auto v1 = vr::findProfile("forward");
    require(v1.has_value(), "v1 alias should resolve");
    require(v1->id == vr::ProfileId::V1SceneForward, "forward alias should map to v1");

    const auto rt = vr::findProfile("rt");
    require(rt.has_value(), "rt alias should resolve");
    require(rt->requiresRayTracing, "rt profile should require ray tracing");

    const auto v4 = vr::findProfile("v4-deferred");
    require(v4.has_value(), "v4 alias should resolve");
    const auto v4Graph = vr::RenderGraph::build(*v4);
    require(!v4Graph.containsRayTracingPass(), "v4 should not contain ray tracing passes");
    require(v4Graph.passes().size() >= 7, "v4 should have deferred pass sequence");

    const auto rtGraph = vr::RenderGraph::build(*rt);
    require(rtGraph.containsRayTracingPass(), "v5 should contain ray tracing passes");
    require(rtGraph.passes().front().name == "scene.upload", "all graphs start with scene upload");

    require(vr::validationProfiles().size() == 5, "expected validation plans for v1 through v5");
    const auto v2Plan = vr::findValidationProfile(vr::ProfileId::V2PbrIbl);
    require(v2Plan.has_value(), "v2 validation plan should exist");
    require(v2Plan->renderer72Version == "Renderer72 v2.0", "v2 plan should map to Renderer72 v2.0");
    require(v2Plan->referenceFeatures.size() >= 6, "v2 plan should track material feature list");
    const auto v4Plan = vr::findValidationProfile(vr::ProfileId::V4DeferredSsao);
    require(v4Plan.has_value(), "v4 validation plan should exist");
    require(v4Plan->steps.front().expectedEvidence.size() >= 5, "v4 plan should include benchmark evidence markers");

    std::cout << "profile graph tests passed\n";
    return 0;
}
