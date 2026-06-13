#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vr {

enum class ProfileId {
    V1SceneForward,
    V2PbrIbl,
    V3LightsShadows,
    V4DeferredSsao,
    V5RealtimeRayTracing
};

struct FeatureProfile {
    ProfileId id;
    std::string_view key;
    std::string_view displayName;
    std::vector<std::string_view> aliases;
    std::vector<std::string_view> capabilities;
    bool requiresRayTracing = false;
};

std::span<const FeatureProfile> featureProfiles();
const FeatureProfile& defaultProfile();
std::optional<FeatureProfile> findProfile(std::string_view key);
std::string profileKeys();

} // namespace vr

