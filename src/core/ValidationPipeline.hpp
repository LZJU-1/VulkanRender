#pragma once

#include "core/FeatureProfile.hpp"

#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace vr {

struct ValidationStep {
    std::string_view name;
    std::string_view referenceFeature;
    std::string_view mode;
    std::string_view command;
    std::vector<std::string_view> expectedEvidence;
    std::vector<std::string_view> visualChecks;
};

struct ValidationProfile {
    ProfileId profileId;
    std::string_view renderer72Version;
    std::string_view title;
    std::string_view scope;
    std::vector<std::string_view> primaryAssets;
    std::vector<std::string_view> referenceFeatures;
    std::vector<ValidationStep> steps;
};

std::span<const ValidationProfile> validationProfiles();
std::optional<ValidationProfile> findValidationProfile(ProfileId id);
void printValidationPipelines(std::ostream& out, std::optional<ProfileId> onlyProfile = std::nullopt);

} // namespace vr
