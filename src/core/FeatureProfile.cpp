#include "core/FeatureProfile.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace vr {
namespace {

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto a = static_cast<unsigned char>(lhs[i]);
        const auto b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

const std::array<FeatureProfile, 5> kProfiles = {{
    {
        ProfileId::V1SceneForward,
        "v1",
        "v1 scene forward",
        {"v1-scene", "scene", "forward"},
        {"scene-loader", "animation", "simple-material", "frustum-culling"},
        false,
    },
    {
        ProfileId::V2PbrIbl,
        "v2",
        "v2 pbr and ibl",
        {"v2-pbr", "pbr", "ibl"},
        {"skybox", "tone-mapping", "normal-mapping", "displacement-mapping", "pbr", "ibl-precompute"},
        false,
    },
    {
        ProfileId::V3LightsShadows,
        "v3",
        "v3 lights and shadows",
        {"v3-shadows", "lights", "shadows"},
        {"spot-light", "sphere-light", "directional-light", "shadow-maps", "cascades"},
        false,
    },
    {
        ProfileId::V4DeferredSsao,
        "v4",
        "v4 deferred and ssao",
        {"v4-deferred", "deferred", "ssao"},
        {"gbuffer", "deferred-shading", "ssao", "many-lights"},
        false,
    },
    {
        ProfileId::V6HybridRealtimeRayTracing,
        "v6-hybrid",
        "v6 reference hybrid realtime ray tracing",
        {"v6", "v6-rt", "v6-hybrid-rt", "hybrid", "hybrid-rt", "rt", "raytracing", "ray-tracing", "realtime-rt", "v5", "v5-rt", "v5-hybrid"},
        {
            "reference-hybrid-render-path",
            "raster-gbuffer",
            "blas",
            "tlas",
            "raytraced-shadows",
            "raytraced-ambient-occlusion",
            "raytraced-reflections",
            "svgf-denoise",
            "temporal-accumulation",
            "adaptive-downsample",
        },
        true,
    },
}};

} // namespace

std::span<const FeatureProfile> featureProfiles() {
    return kProfiles;
}

const FeatureProfile& defaultProfile() {
    return kProfiles.front();
}

std::optional<FeatureProfile> findProfile(std::string_view key) {
    for (const auto& profile : kProfiles) {
        if (equalsIgnoreCase(profile.key, key)) {
            return profile;
        }
        const auto found = std::find_if(
            profile.aliases.begin(),
            profile.aliases.end(),
            [key](std::string_view alias) { return equalsIgnoreCase(alias, key); }
        );
        if (found != profile.aliases.end()) {
            return profile;
        }
    }
    return std::nullopt;
}

std::string profileKeys() {
    std::ostringstream out;
    bool first = true;
    for (const auto& profile : kProfiles) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << profile.key;
    }
    return out.str();
}

} // namespace vr
