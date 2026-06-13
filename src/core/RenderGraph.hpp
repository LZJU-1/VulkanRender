#pragma once

#include "core/FeatureProfile.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace vr {

enum class PassKind {
    Setup,
    Raster,
    Compute,
    RayTracing,
    PostProcess,
    Readback
};

struct RenderPassInfo {
    std::string name;
    PassKind kind;
    bool optional = false;
};

class RenderGraph {
public:
    static RenderGraph build(const FeatureProfile& profile);

    const FeatureProfile& profile() const { return profile_; }
    const std::vector<RenderPassInfo>& passes() const { return passes_; }
    bool containsRayTracingPass() const;
    void describe(std::ostream& out) const;

private:
    explicit RenderGraph(FeatureProfile profile);
    void add(std::string name, PassKind kind, bool optional = false);

    FeatureProfile profile_;
    std::vector<RenderPassInfo> passes_;
};

std::string toString(PassKind kind);

} // namespace vr

