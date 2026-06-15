#include "core/ValidationPipeline.hpp"

#include <array>
#include <ostream>

namespace vr {
namespace {

const std::array<ValidationProfile, 5> kValidationProfiles = {{
    {
        ProfileId::V1SceneForward,
        "Renderer72 v1.0",
        "v1 scene forward validation",
        "Scene loader, simple material, animation, and frustum culling.",
        {
            "assets/scenes/v1.scene",
            "assets/third_party/s72_examples/rotation.s72",
            "assets/third_party/s72_examples/sg-Articulation.s72",
        },
        {
            "Scene loader",
            "Simple material",
            "Animation",
            "Frustum culling",
        },
        {
            {
                "Built-in cube smoke render",
                "Simple material + frustum culling",
                "headless render",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v1 --render --scene assets\\scenes\\v1.scene --output out\\validation-v1-cubes.bmp --width 1280 --height 720 --frames 16",
                {"Rendered v1 image", "objects=", "visible=", "triangles="},
                {"Image contains the cube scene.", "One intentionally off-frustum object is culled."},
            },
            {
                "Official Scene72 transform smoke",
                "Scene loader",
                "headless render",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v1 --render --scene assets\\third_party\\s72_examples\\rotation.s72 --output out\\validation-v1-rotation.bmp --width 960 --height 960",
                {"Rendered v1 image", "triangles="},
                {"Rotation scene geometry appears with expected vertex colors."},
            },
            {
                "Official driver animation preview",
                "Animation",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v1 --preview --scene assets\\third_party\\s72_examples\\sg-Articulation.s72 --width 1280 --height 720",
                {"Profile: v1 scene forward", "Frame 0:"},
                {"Articulated arm preserves hierarchy.", "Driver animation loops in the preview window."},
            },
        },
    },
    {
        ProfileId::V2PbrIbl,
        "Renderer72 v2.0",
        "v2 PBR and IBL validation",
        "Skybox, tone mapping, normal mapping, displacement mapping, PBR material, and IBL.",
        {
            "assets/third_party/s72_examples/materials.s72",
            "assets/third_party/s72_examples/ox_bridge_morning.png",
            "assets/third_party/ambientcg/Rock064_1K-JPG",
        },
        {
            "Skybox",
            "Tone mapping",
            "Normal mapping",
            "Displacement mapping",
            "PBR material",
            "IBL",
            "Precomputing environment map",
        },
        {
            {
                "Software material smoke render",
                "PBR material",
                "headless render",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v2 --render --scene assets\\third_party\\s72_examples\\materials.s72 --output out\\validation-v2-materials.bmp --width 1280 --height 720",
                {"Rendered v2 image", "objects=", "triangles="},
                {"Material spheres and labels are present in the BMP."},
            },
            {
                "GPU material and environment preview",
                "Skybox + tone mapping + PBR material + IBL",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v2 --preview --scene assets\\third_party\\s72_examples\\materials.s72 --width 1280 --height 720",
                {"createTexture", "draw/present", "materialSets="},
                {"Skybox uses the Ox Bridge cubemap.", "Mirror and low-roughness spheres reflect the environment.", "Tone mapping keeps highlights from clipping."},
            },
            {
                "Texture detail preview",
                "Normal mapping + displacement mapping",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v2 --preview --scene assets\\third_party\\s72_examples\\materials.s72 --width 1280 --height 720",
                {"draw/present"},
                {"Rock material shows stable normal detail.", "Parallax displacement fades smoothly at distance.", "Texture minification is stable while roaming."},
            },
        },
    },
    {
        ProfileId::V3LightsShadows,
        "Renderer72 v3.0",
        "v3 lights and shadows validation",
        "Spot light perspective shadows, sphere/point omnidirectional shadows, and directional cascades.",
        {
            "assets/third_party/s72_examples/v3_shadow_demo.shadowdemo",
            "assets/third_party/s72_examples/materials.s72",
        },
        {
            "Spot light",
            "Perspective shadow mapping",
            "Sphere light",
            "Omnidirectional shadow mapping",
            "Directional light",
            "Cascade shadow mapping",
            "Cascade visualization",
        },
        {
            {
                "Default shadow atlas preview",
                "Spot/sphere/directional shadow maps",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v3 --preview --width 1280 --height 720",
                {"createShadowRenderPass", "createShadowResources", "draw/present"},
                {"Directional shadows land on the floor and wall.", "Spot cone casts a perspective shadow.", "Point/sphere light occlusion changes by direction."},
            },
            {
                "Cascade stability pass",
                "Cascade shadow mapping",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v3 --preview --scene assets\\third_party\\s72_examples\\v3_shadow_demo.shadowdemo --width 1280 --height 720",
                {"draw/present"},
                {"Near shadows remain crisp while roaming.", "Far blockers remain covered by a cascade."},
            },
            {
                "Material regression under v3 lighting",
                "Lights combined with v2 PBR/IBL",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v3 --preview --scene assets\\third_party\\s72_examples\\materials.s72 --width 1280 --height 720",
                {"draw/present"},
                {"v2 material spheres still shade correctly with the v3 profile enabled."},
            },
        },
    },
    {
        ProfileId::V4DeferredSsao,
        "Renderer72 v4.0",
        "v4 deferred and SSAO validation",
        "Deferred shading, SSAO, and a 1024-light / 10000-sphere benchmark scene.",
        {
            "assets/third_party/s72_examples/v4_many_lights.manylights",
            "assets/third_party/s72_examples/v3_shadow_demo.shadowdemo",
        },
        {
            "Deferred shading",
            "SSAO",
            "1024 sphere lights",
            "10000 PBR spheres",
        },
        {
            {
                "Default many-light deferred preview",
                "Deferred shading + 1024 sphere lights + 10000 PBR spheres",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v4 --preview --width 1280 --height 720",
                {"createGBufferRenderPass", "createSsaoRenderPass", "createV4ComposeDescriptors", "lights=1024", "sphereInstances=10000", "draw/present"},
                {"Dense PBR sphere grid is visible.", "Colored local lights affect nearby spheres.", "No black or blank frame during presentation."},
            },
            {
                "SSAO contact regression",
                "SSAO",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v4 --preview --scene assets\\third_party\\s72_examples\\v3_shadow_demo.shadowdemo --width 1280 --height 720",
                {"createSsaoResources", "draw/present"},
                {"Contact darkening appears at floor/object intersections.", "SSAO raw and blur debug views are available on number keys 4 and 5."},
            },
            {
                "G-buffer debug regression",
                "Deferred shading",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v4 --preview --width 1280 --height 720",
                {"draw/present"},
                {"Debug views 1, 2, and 3 show albedo, normal, and depth buffers."},
            },
        },
    },
    {
        ProfileId::V5RealtimeRayTracing,
        "Local v5 extension",
        "v5 realtime ray tracing validation",
        "Project extension beyond Renderer72 README: hybrid G-buffer plus Vulkan ray-query shadows and temporal accumulation.",
        {
            "assets/third_party/s72_examples/materials.s72",
            "C:\\Users\\lzju\\Desktop\\MonteCarloPathTracer\\scenes\\bathroom2\\bathroom2.xml",
        },
        {
            "BLAS/TLAS build",
            "Ray-query shadows",
            "Temporal accumulation",
            "Swapchain storage output",
            "OBJ/MTL/XML scene import",
        },
        {
            {
                "Materials RT smoke preview",
                "Ray-query shadows",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v5-rt --preview --scene assets\\third_party\\s72_examples\\materials.s72 --width 1280 --height 720",
                {"createGBufferRenderPass", "createV5HistoryResources", "createV5AccelerationStructures", "createV5RayTracingDescriptors", "v5RayTracing=on", "draw/present"},
                {"Soft RT shadow noise settles when the camera is still.", "Camera motion resets accumulation without smearing."},
            },
            {
                "Bathroom OBJ/XML import preview",
                "OBJ/MTL/XML scene import",
                "realtime preview",
                "build\\nmake-debug\\src\\vulkan_render.exe --profile v5-rt --preview --scene C:\\Users\\lzju\\Desktop\\MonteCarloPathTracer\\scenes\\bathroom2\\bathroom2.xml --width 1280 --height 720",
                {"geometry vertices=", "tlas=ready", "lights=", "v5RayTracing=on", "draw/present"},
                {"Bathroom mesh loads with imported camera.", "Emissive surfaces contribute bounded realtime lights."},
            },
        },
    },
}};

void printStringList(std::ostream& out, std::string_view label, const std::vector<std::string_view>& values) {
    out << "  " << label << ":\n";
    for (const auto value : values) {
        out << "    - " << value << '\n';
    }
}

} // namespace

std::span<const ValidationProfile> validationProfiles() {
    return kValidationProfiles;
}

std::optional<ValidationProfile> findValidationProfile(ProfileId id) {
    for (const auto& profile : kValidationProfiles) {
        if (profile.profileId == id) {
            return profile;
        }
    }
    return std::nullopt;
}

void printValidationPipelines(std::ostream& out, std::optional<ProfileId> onlyProfile) {
    out << "Renderer72-aligned validation pipeline\n";
    out << "Reference README versions: v1.0 scene/animation/culling, v2.0 material/IBL, v3.0 lights/shadows, v4.0 deferred/SSAO.\n";
    out << "Use v5 as this repo's realtime ray tracing extension.\n";

    for (const auto& profile : kValidationProfiles) {
        if (onlyProfile && profile.profileId != *onlyProfile) {
            continue;
        }

        out << "\n[" << profile.renderer72Version << "] " << profile.title << '\n';
        out << "  Scope: " << profile.scope << '\n';
        printStringList(out, "Reference features", profile.referenceFeatures);
        printStringList(out, "Primary assets", profile.primaryAssets);
        out << "  Validation steps:\n";
        for (const auto& step : profile.steps) {
            out << "    - " << step.name << '\n';
            out << "      Reference: " << step.referenceFeature << '\n';
            out << "      Mode: " << step.mode << '\n';
            out << "      Command: " << step.command << '\n';
            if (!step.expectedEvidence.empty()) {
                out << "      Expected evidence:\n";
                for (const auto evidence : step.expectedEvidence) {
                    out << "        * " << evidence << '\n';
                }
            }
            if (!step.visualChecks.empty()) {
                out << "      Visual checks:\n";
                for (const auto check : step.visualChecks) {
                    out << "        * " << check << '\n';
                }
            }
        }
    }
}

} // namespace vr
