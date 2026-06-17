// V5 Hardware RT Pipeline: raygen + miss + closest-hit
// Replaces ray-query compute shader — uses VK_KHR_ray_tracing_pipeline

#include "v5_shared.hlsl"

cbuffer Camera : register(b0) {
    float4 eyeNear;
    float4 rightFar;
    float4 upTanHalf;
    float4 forwardAspect;
    float4 shadowRightExtent;
    float4 shadowUpNear;
    float4 shadowForwardFar;
    float4 shadowCenterBias;
    float4 pointPosRadius;
    float4 pointColorIntensity;
    float4 spotPosInner;
    float4 spotDirOuter;
    float4 spotColorIntensity;
    float4 v3Flags;
    float4 v4Flags;
    float4 taaJitter;
    float4 prevEyeNear;
    float4 prevRightFar;
    float4 prevUpTanHalf;
    float4 prevForwardAspect;
    float4 prevTaaJitter;
};

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(15, 0)]] Texture2D<float4> gbufferAlbedo;
[[vk::binding(16, 0)]] Texture2D<float4> gbufferNormal;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;
[[vk::binding(21, 0)]] RaytracingAccelerationStructure sceneTlas;
[[vk::binding(24, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> shadowSignal : register(u24);
[[vk::binding(25, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> reflectionSignal : register(u25);
[[vk::binding(20, 0)]] StructuredBuffer<SceneLight> sceneLights;

struct RayPayload { float3 color; float hitDistance; uint recursionDepth; };
struct ShadowPayload { float visibility; };

float3 cheapLighting(Surface surface, float3 viewDir) {
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float ndotl = saturate(dot(surface.normal, lightDir));
    float3 ambient = surface.base * 0.22;
    return ambient + surface.base * ndotl * 0.78;
}

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (float2(pixel) + 0.5) / float2(dims);

    // Read G-buffer
    float4 worldKind = gbufferWorld.Load(int3(pixel, 0));
    if (worldKind.w <= 0.5) {
        shadowSignal[pixel] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionSignal[pixel] = float4(0.0.xxx, 0.0);
        return;
    }

    float4 albedoRoughness = gbufferAlbedo.Load(int3(pixel, 0));
    float4 normalMetalness = gbufferNormal.Load(int3(pixel, 0));
    Surface surface;
    surface.worldPos = worldKind.xyz;
    surface.normal = normalize(normalMetalness.rgb * 2.0 - 1.0);
    surface.base = saturate(albedoRoughness.rgb);
    surface.roughness = clamp(albedoRoughness.a, 0.035, 1.0);
    surface.metalness = saturate(normalMetalness.a);
    surface.materialKind = max(0.0, worldKind.w - 1.0);

    // Emissive materials
    if (abs(surface.materialKind - 5.0) < 0.25) {
        shadowSignal[pixel] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionSignal[pixel] = float4(0.0.xxx, 0.0);
        return;
    }

    float3 viewDir = normalize(eyeNear.xyz - surface.worldPos);

    // --- Shadow ray ---
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    ShadowPayload shadowPayload = { 1.0 };
    RayDesc shadowRay;
    shadowRay.Origin = surface.worldPos + surface.normal * 0.05;
    shadowRay.Direction = lightDir;
    shadowRay.TMin = 0.04;
    shadowRay.TMax = max(rightFar.w, 32.0);
    TraceRay(sceneTlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xFF, 0, 0, 0, shadowRay, shadowPayload);

    // Local lights
    float3 localDirect = 0.0.xxx;
    uint lightCount = min((uint)max(0.0, v4Flags.y), 128u);
    for (uint i = 0; i < lightCount; ++i) {
        SceneLight light = sceneLights[i];
        float3 toLight = light.positionRadius.xyz - surface.worldPos;
        float d = length(toLight);
        float3 lDir = toLight / max(d, 0.001);
        float ndotl = saturate(dot(surface.normal, lDir));
        if (ndotl <= 0.0) continue;

        ShadowPayload lp = { 1.0 };
        RayDesc lr;
        lr.Origin = surface.worldPos + surface.normal * 0.03;
        lr.Direction = lDir;
        lr.TMin = 0.02;
        lr.TMax = max(0.04, d - 0.05);
        TraceRay(sceneTlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
            0xFF, 0, 0, 0, lr, lp);

        float falloff = 1.0 - saturate(d / max(0.25, light.positionRadius.w));
        falloff = falloff * falloff * (3.0 - 2.0 * falloff);
        localDirect += pbrDirectional(surface, viewDir, lDir,
            saturate(light.colorIntensity.rgb),
            light.colorIntensity.w * falloff, lp.visibility);
    }

    shadowSignal[pixel] = float4(shadowPayload.visibility, localDirect);

    // --- Reflection ray ---
    float smoothness = saturate((0.58 - surface.roughness) / 0.58);
    float reflWeight = (surface.materialKind > 1.5 && surface.materialKind < 2.5) ? 1.0 :
        smoothness * smoothness * lerp(0.025, 0.86, surface.metalness);
    if (reflWeight < 0.04) {
        reflectionSignal[pixel] = float4(0.0.xxx, 0.0);
        return;
    }

    float3 reflDir = normalize(reflect(-viewDir, surface.normal));
    RayPayload reflPayload = { 0.0.xxx, 0.0, 0 };
    RayDesc reflRay;
    reflRay.Origin = surface.worldPos + surface.normal * 0.06;
    reflRay.Direction = reflDir;
    reflRay.TMin = 0.05;
    reflRay.TMax = max(rightFar.w, 48.0);
    TraceRay(sceneTlas, RAY_FLAG_NONE, 0xFF, 0, 1, 0, reflRay, reflPayload);

    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(saturate(dot(surface.normal, viewDir)), f0);
    reflectionSignal[pixel] = float4(reflPayload.color, saturate(reflWeight * dot(fresnel, float3(0.2126, 0.7152, 0.0722))));
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float3(0.48, 0.50, 0.52) * 0.5;
    payload.hitDistance = 1e6;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload) {
    payload.visibility = 1.0;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    if (payload.recursionDepth > 0) return;

    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    // Project hit to G-buffer for material lookup
    float3 rel = hitPos - eyeNear.xyz;
    float camZ = dot(rel, forwardAspect.xyz);
    if (camZ <= 0.01) return;

    float tanHalf = max(upTanHalf.w, 0.001);
    float aspect = max(forwardAspect.w, 0.001);
    float2 ndc;
    ndc.x = dot(rel, rightFar.xyz) / (camZ * aspect * tanHalf);
    ndc.y = -dot(rel, upTanHalf.xyz) / (camZ * tanHalf);
    float2 uv = (ndc + taaJitter.xy) * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return;

    uint2 gbufDims;
    gbufferWorld.GetDimensions(gbufDims.x, gbufDims.y);
    int2 pixel = int2(uv * float2(gbufDims));
    float4 worldKind = gbufferWorld.Load(int3(pixel, 0));
    if (worldKind.w <= 0.5) return;

    float3 gbufPos = worldKind.xyz;
    float gbufDepth = dot(gbufPos - eyeNear.xyz, forwardAspect.xyz);
    if (abs(camZ - gbufDepth) > max(0.15, camZ * 0.06)) return;

    float4 albedoRoughness = gbufferAlbedo.Load(int3(pixel, 0));
    float4 normalMetalness = gbufferNormal.Load(int3(pixel, 0));
    float3 n = normalize(normalMetalness.rgb * 2.0 - 1.0);
    float3 base = saturate(albedoRoughness.rgb);

    float3 viewDir = normalize(eyeNear.xyz - gbufPos);
    Surface hitSurface;
    hitSurface.worldPos = gbufPos;
    hitSurface.normal = n;
    hitSurface.base = base;
    hitSurface.roughness = clamp(albedoRoughness.a, 0.035, 1.0);
    hitSurface.metalness = saturate(normalMetalness.a);
    hitSurface.materialKind = max(0.0, worldKind.w - 1.0);
    payload.color = cheapLighting(hitSurface, viewDir);
    payload.hitDistance = RayTCurrent();
}
