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

[[vk::binding(1, 0), vk::image_format("rgba8")]]
RWTexture2D<float4> outputImage : register(u1);

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(15, 0)]] Texture2D<float4> gbufferAlbedo;
[[vk::binding(16, 0)]] Texture2D<float4> gbufferNormal;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;
[[vk::binding(21, 0)]] RaytracingAccelerationStructure sceneTlas;
[[vk::binding(22, 0)]] Texture2D<float4> historyColor;
[[vk::binding(23, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> historyOutput : register(u23);
[[vk::binding(24, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> shadowSignal : register(u24);
[[vk::binding(25, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> reflectionSignal : register(u25);

#include "v5_shared.hlsl"

[[vk::binding(20, 0)]] StructuredBuffer<SceneLight> sceneLights;

float3 skyColorAtUv(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0 - taaJitter.xy;
    float3 cameraRay = normalize(forwardAspect.xyz + rightFar.xyz * ndc.x * forwardAspect.w * upTanHalf.w + upTanHalf.xyz * -ndc.y * upTanHalf.w);
    return toneMap(skyRadiance(cameraRay));
}

bool readSurface(float2 uv, out Surface surface) {
    surface.worldPos = 0.0.xxx;
    surface.normal = float3(0.0, 0.0, 1.0);
    surface.base = 0.0.xxx;
    surface.roughness = 1.0;
    surface.metalness = 0.0;
    surface.materialKind = 0.0;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return false;
    }

    uint width;
    uint height;
    gbufferWorld.GetDimensions(width, height);
    int2 pixel = int2(min(float2(max(width, 1u), max(height, 1u)) - 1.0.xx, max(0.0.xx, uv * float2(width, height))));

    float4 worldKind = gbufferWorld.Load(int3(pixel, 0));
    if (worldKind.w <= 0.5) {
        return false;
    }

    float4 albedoRoughness = gbufferAlbedo.Load(int3(pixel, 0));
    float4 normalMetalness = gbufferNormal.Load(int3(pixel, 0));
    surface.worldPos = worldKind.xyz;
    surface.normal = normalize(normalMetalness.rgb * 2.0 - 1.0);
    surface.base = saturate(albedoRoughness.rgb);
    surface.roughness = clamp(albedoRoughness.a, 0.035, 1.0);
    surface.metalness = saturate(normalMetalness.a);
    surface.materialKind = max(0.0, worldKind.w - 1.0);
    return true;
}

float3 importedLightsRadiance(Surface surface, float3 view, uint2 pixel);

float3 cheapNeighborColor(uint2 pixel, uint width, uint height) {
    float2 uv = (float2(pixel) + 0.5) / float2(max(width, 1u), max(height, 1u));
    Surface surface;
    if (!readSurface(uv, surface)) {
        return skyColorAtUv(uv);
    }
    float3 view = normalize(eyeNear.xyz - surface.worldPos);
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float ndotl = saturate(dot(surface.normal, lightDir));
    float3 lit = surface.base * (0.20 + ndotl * 0.70);
    if (abs(surface.materialKind - 5.0) < 0.25) {
        lit = surface.base * 8.0;
    }
    return toneMap(lit + importedLightsRadiance(surface, view, pixel));
}

float edgeStrength(uint2 pixel, uint width, uint height) {
    int2 centerPixel = int2(pixel);
    int2 maxPixel = int2(max(width, 1u) - 1u, max(height, 1u) - 1u);
    float4 centerWorld = gbufferWorld.Load(int3(centerPixel, 0));
    if (centerWorld.w <= 0.5) {
        return 0.0;
    }

    float3 centerNormal = normalize(gbufferNormal.Load(int3(centerPixel, 0)).rgb * 2.0 - 1.0);
    float centerDepth = dot(centerWorld.xyz - eyeNear.xyz, forwardAspect.xyz);
    float edge = 0.0;

    [unroll]
    for (int i = 0; i < 4; ++i) {
        int2 offset = i == 0 ? int2(1, 0) : (i == 1 ? int2(-1, 0) : (i == 2 ? int2(0, 1) : int2(0, -1)));
        int2 samplePixel = min(max(centerPixel + offset, int2(0, 0)), maxPixel);
        float4 sampleWorld = gbufferWorld.Load(int3(samplePixel, 0));
        float objectEdge = abs(sampleWorld.w - centerWorld.w) > 0.25 ? 1.0 : 0.0;
        float sampleDepth = dot(sampleWorld.xyz - eyeNear.xyz, forwardAspect.xyz);
        float depthEdge = smoothstep(0.05, 0.45, abs(sampleDepth - centerDepth));
        float3 sampleNormal = normalize(gbufferNormal.Load(int3(samplePixel, 0)).rgb * 2.0 - 1.0);
        float normalEdge = smoothstep(0.18, 0.65, 1.0 - saturate(dot(centerNormal, sampleNormal)));
        edge = max(edge, max(objectEdge, max(depthEdge, normalEdge)));
    }

    return edge;
}

float3 applyEdgeAntialias(uint2 pixel, uint width, uint height, float3 color) {
    float edge = edgeStrength(pixel, width, height);
    if (edge <= 0.001) {
        return color;
    }

    int2 centerPixel = int2(pixel);
    int2 maxPixel = int2(max(width, 1u) - 1u, max(height, 1u) - 1u);
    uint2 rightPixel = uint2(min(max(centerPixel + int2(1, 0), int2(0, 0)), maxPixel));
    uint2 leftPixel = uint2(min(max(centerPixel + int2(-1, 0), int2(0, 0)), maxPixel));
    uint2 downPixel = uint2(min(max(centerPixel + int2(0, 1), int2(0, 0)), maxPixel));
    uint2 upPixel = uint2(min(max(centerPixel + int2(0, -1), int2(0, 0)), maxPixel));
    float3 neighborAverage =
        cheapNeighborColor(rightPixel, width, height) +
        cheapNeighborColor(leftPixel, width, height) +
        cheapNeighborColor(downPixel, width, height) +
        cheapNeighborColor(upPixel, width, height);
    neighborAverage *= 0.25;
    return lerp(color, neighborAverage, min(0.34, edge * 0.26));
}

bool projectWorldToUv(float3 p, out float2 uv, out float cameraZ) {
    float3 rel = p - eyeNear.xyz;
    cameraZ = dot(rel, forwardAspect.xyz);
    if (cameraZ <= max(eyeNear.w, 0.01)) {
        uv = 0.0.xx;
        return false;
    }

    float tanHalf = max(upTanHalf.w, 0.001);
    float aspect = max(forwardAspect.w, 0.001);
    float2 ndc;
    ndc.x = dot(rel, rightFar.xyz) / (cameraZ * aspect * tanHalf);
    ndc.y = -dot(rel, upTanHalf.xyz) / (cameraZ * tanHalf);
    uv = (ndc + taaJitter.xy) * 0.5 + 0.5;
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool traceGBufferRay(float3 origin, float3 dir, float maxDistance, int steps, float baseThickness, out Surface hitSurface, out float hitDistance) {
    hitDistance = maxDistance;
    float stepLength = maxDistance / max((float)steps, 1.0);
    float startDistance = max(0.05, stepLength * 0.65);

    [loop]
    for (int i = 0; i < 96; ++i) {
        if (i >= steps) {
            break;
        }

        float distanceOnRay = startDistance + ((float)i + 0.5) * stepLength;
        float3 samplePoint = origin + dir * distanceOnRay;
        float2 uv;
        float rayDepth;
        if (!projectWorldToUv(samplePoint, uv, rayDepth)) {
            continue;
        }

        Surface candidate;
        if (!readSurface(uv, candidate)) {
            continue;
        }

        float candidateDepth = dot(candidate.worldPos - eyeNear.xyz, forwardAspect.xyz);
        float depthDelta = rayDepth - candidateDepth;
        float thickness = baseThickness + rayDepth * 0.018;
        if (depthDelta > 0.0 && depthDelta < thickness) {
            hitSurface = candidate;
            hitDistance = distanceOnRay;
            return true;
        }
    }

    return false;
}

float hash13(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float2 rotate2(float2 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void writeAccumulatedColor(uint2 pixel, float3 currentColor) {
    uint width;
    uint height;
    outputImage.GetDimensions(width, height);
    int2 centerPixel = int2(pixel);
    int2 maxPixel = int2(max(width, 1u) - 1u, max(height, 1u) - 1u);
    uint2 rightPixel = uint2(min(max(centerPixel + int2(1, 0), int2(0, 0)), maxPixel));
    uint2 leftPixel = uint2(min(max(centerPixel + int2(-1, 0), int2(0, 0)), maxPixel));
    uint2 downPixel = uint2(min(max(centerPixel + int2(0, 1), int2(0, 0)), maxPixel));
    uint2 upPixel = uint2(min(max(centerPixel + int2(0, -1), int2(0, 0)), maxPixel));
    float3 neighborhoodMin = currentColor;
    float3 neighborhoodMax = currentColor;
    float3 n0 = cheapNeighborColor(rightPixel, width, height);
    float3 n1 = cheapNeighborColor(leftPixel, width, height);
    float3 n2 = cheapNeighborColor(downPixel, width, height);
    float3 n3 = cheapNeighborColor(upPixel, width, height);
    neighborhoodMin = min(neighborhoodMin, min(min(n0, n1), min(n2, n3)));
    neighborhoodMax = max(neighborhoodMax, max(max(n0, n1), max(n2, n3)));

    float historyFrames = max(v4Flags.w, 0.0);
    float3 previousColor = clamp(historyColor.Load(int3(pixel, 0)).rgb, neighborhoodMin, neighborhoodMax);
    float lumaCurrent = dot(currentColor, float3(0.2126, 0.7152, 0.0722));
    float lumaPrevious = dot(previousColor, float3(0.2126, 0.7152, 0.0722));
    float historyConfidence = 1.0 - saturate(abs(lumaCurrent - lumaPrevious) * 4.0);
    float historyWeight = historyFrames < 1.0 ? 0.0 : min(0.92, historyFrames / (historyFrames + 1.0));
    historyWeight *= historyConfidence;
    float3 accumulated = lerp(currentColor, previousColor, historyWeight);
    historyOutput[pixel] = float4(accumulated, 1.0);
    outputImage[pixel] = float4(applyEdgeAntialias(pixel, width, height, accumulated), 1.0);
}

bool traceSceneRay(float3 origin, float3 rayDir, float minDistance, float maxDistance, out float hitDistance) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
    ray.TMin = minDistance;
    ray.TMax = max(maxDistance, minDistance + 0.001);

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(
        sceneTlas,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE,
        0xff,
        ray
    );
    while (query.Proceed()) {
    }

    bool hit = query.CommittedStatus() != COMMITTED_NOTHING;
    hitDistance = hit ? query.CommittedRayT() : maxDistance;
    return hit;
}

float traceShadowRay(float3 origin, float3 normal, float3 rayDir, float maxDistance) {
    float hitDistance;
    bool hit = traceSceneRay(origin + normal * 0.05, rayDir, 0.035, maxDistance, hitDistance);
    return hit ? 0.0 : 1.0;
}

float rayTracedVisibility(float3 origin, float3 normal, float3 lightDir, uint2 pixel) {
    if (dot(normal, lightDir) <= 0.02) {
        return 1.0;
    }

    float3 helper = abs(lightDir.z) < 0.95 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 tangent = normalize(cross(helper, lightDir));
    float3 bitangent = cross(lightDir, tangent);
    float rotation = hash13(origin * 0.137 + float3((float)pixel.x, (float)pixel.y, v4Flags.w * 17.0 + 3.1) * 0.021) * 2.0 * kPi;
    float angularRadius = lerp(0.026, 0.064, saturate(length(origin - eyeNear.xyz) / 14.0));

    static const float2 poisson[4] = {
        float2(0.000, 0.000),
        float2(0.527, 0.086),
        float2(-0.327, 0.421),
        float2(-0.481, -0.221),
    };

    float unoccluded = 0.0;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        float2 disk = rotate2(poisson[i], rotation);
        float3 rayDir = normalize(lightDir + (tangent * disk.x + bitangent * disk.y) * angularRadius);
        unoccluded += traceShadowRay(origin, normal, rayDir, max(rightFar.w, 32.0));
    }

    float visibility = unoccluded / 4.0;
    return lerp(0.08, 1.0, smoothstep(0.0, 1.0, visibility));
}

float rayTracedAmbientOcclusion(float3 origin, float3 normal) {
    float3 up = abs(normal.z) < 0.96 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    float occlusion = 0.0;

    [unroll]
    for (int i = 0; i < 6; ++i) {
        float angle = ((float)i + 0.5) * 2.39996323;
        float radius = ((float)i + 1.0) / 6.0;
        float3 localDir = normalize(tangent * cos(angle) * radius + bitangent * sin(angle) * radius + normal * (0.55 + 0.45 * radius));
        Surface blocker;
        float hitDistance;
        if (traceGBufferRay(origin + normal * 0.04, localDir, 0.65, 8, 0.035, blocker, hitDistance)) {
            occlusion += saturate(1.0 - hitDistance / 0.65);
        }
    }

    return saturate(1.0 - occlusion / 12.0);
}

float3 rayTracedReflection(Surface surface, float3 cameraToSurface, float3 view) {
    float3 reflectionDir = normalize(reflect(cameraToSurface, surface.normal));
    float smoothness = saturate((0.58 - surface.roughness) / 0.58);
    if (smoothness * smoothness * lerp(0.12, 1.0, surface.metalness) < 0.08) {
        return skyRadiance(reflectionDir) * 0.35;
    }

    const float tlasTmin = 0.06;
    const float tlasNormalBias = 0.08;
    float sceneHitDistance;
    bool sceneHit = traceSceneRay(surface.worldPos + surface.normal * tlasNormalBias, reflectionDir, tlasTmin, max(rightFar.w, 48.0), sceneHitDistance);
    if (sceneHit) {
        float3 firstOrigin = surface.worldPos + surface.normal * tlasNormalBias;
        float3 hitPoint = firstOrigin + reflectionDir * sceneHitDistance;
        float2 hitUv;
        float hitCameraZ;
        if (projectWorldToUv(hitPoint, hitUv, hitCameraZ)) {
            Surface projectedHit;
            if (readSurface(hitUv, projectedHit)) {
                float projectedDepth = dot(projectedHit.worldPos - eyeNear.xyz, forwardAspect.xyz);
                if (abs(hitCameraZ - projectedDepth) < max(0.12, hitCameraZ * 0.04)) {
                    float3 reflectedView = normalize(eyeNear.xyz - projectedHit.worldPos);
                    float3 lightDir = normalize(-shadowForwardFar.xyz);
                    float visibility = rayTracedVisibility(projectedHit.worldPos, projectedHit.normal, lightDir, uint2(hitUv * 8192.0));
                    float3 direct = pbrDirectional(projectedHit, reflectedView, lightDir, float3(1.10, 1.04, 0.92), 2.0, visibility);
                    direct += importedLightsRadiance(projectedHit, reflectedView, uint2(hitUv * 8192.0));
                    return projectedHit.base * 0.20 + direct;
                }
            }
        }
        return float3(0.48, 0.50, 0.52) * lerp(0.5, 1.2, smoothness);
    }

    Surface hitSurface;
    float hitDistance;
    float maxDistance = lerp(5.0, 1.4, surface.roughness);
    bool hit = traceGBufferRay(surface.worldPos + surface.normal * 0.065, reflectionDir, maxDistance, 36, 0.055, hitSurface, hitDistance);
    if (!hit) {
        return skyRadiance(reflectionDir);
    }

    float3 reflectedView = normalize(eyeNear.xyz - hitSurface.worldPos);
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float visibility = 1.0;
    float3 direct = pbrDirectional(hitSurface, reflectedView, lightDir, float3(1.10, 1.04, 0.92), 3.0, visibility);
    direct += importedLightsRadiance(hitSurface, reflectedView, uint2(0, 0));
    float3 ambient = hitSurface.base * 0.22;
    return ambient + direct;
}

float4 pbrLightSignal(Surface surface, float3 view, SceneLight light, uint2 pixel) {
    float3 lightPosition = light.positionRadius.xyz;
    if (light.normalArea.w > 0.0001) {
        float3 lightNormal = normalize(light.normalArea.xyz);
        float3 helper = abs(lightNormal.z) < 0.95 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
        float3 tangent = normalize(cross(helper, lightNormal));
        float3 bitangent = cross(lightNormal, tangent);
        float seed = v4Flags.w * 19.37 + (float)pixel.x * 0.013 + (float)pixel.y * 0.017;
        float u0 = hash13(surface.worldPos * 0.173 + float3(seed, 1.7, 4.1));
        float u1 = hash13(surface.worldPos * 0.241 + float3(3.3, seed, 8.9));
        float diskRadius = max(light.positionRadius.w, sqrt(light.normalArea.w / kPi));
        float r = sqrt(u0) * diskRadius;
        float theta = u1 * 2.0 * kPi;
        lightPosition += (tangent * cos(theta) + bitangent * sin(theta)) * r;

        float3 toSample = lightPosition - surface.worldPos;
        float distanceToSample = length(toSample);
        float3 sampleDir = toSample / max(distanceToSample, 0.001);
        float emissionCos = saturate(dot(lightNormal, -sampleDir));
        float ndotl = saturate(dot(surface.normal, sampleDir));
        if (emissionCos <= 0.0 || ndotl <= 0.0) {
            return float4(0.0.xxxx);
        }

        float visibility = traceShadowRay(surface.worldPos, surface.normal, sampleDir, max(0.04, distanceToSample - 0.025));
        float attenuation = emissionCos * light.normalArea.w / max(distanceToSample * distanceToSample, 0.05);
        float3 radiance = pbrDirectional(surface, view, sampleDir, saturate(light.colorIntensity.rgb), light.colorIntensity.w * attenuation / kPi, visibility);
        return float4(radiance, visibility);
    }

    float3 toLight = lightPosition - surface.worldPos;
    float distanceToLight = length(toLight);
    float radius = max(0.25, light.positionRadius.w);
    float3 lightDir = toLight / max(distanceToLight, 0.001);
    float ndotl = saturate(dot(surface.normal, lightDir));
    if (ndotl <= 0.0 || distanceToLight >= radius) {
        return float4(0.0.xxxx);
    }

    float visibility = traceShadowRay(surface.worldPos, surface.normal, lightDir, max(0.04, distanceToLight - 0.35));
    float falloff = saturate(1.0 - distanceToLight / radius);
    falloff = falloff * falloff * (3.0 - 2.0 * falloff);
    float softEdge = lerp(0.35, 1.0, visibility);
    float3 radiance = pbrDirectional(surface, view, lightDir, saturate(light.colorIntensity.rgb), light.colorIntensity.w * falloff, softEdge);
    return float4(radiance, visibility);
}

float3 pbrPointLight(Surface surface, float3 view, SceneLight light, uint2 pixel) {
    return pbrLightSignal(surface, view, light, pixel).rgb;
}

float4 importedLightsSignal(Surface surface, float3 view, uint2 pixel) {
    uint lightCount = min((uint)max(0.0, v4Flags.y), 128u);
    float3 radiance = 0.0.xxx;
    float visibility = 0.0;
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        float4 signal = pbrLightSignal(surface, view, sceneLights[i], pixel);
        radiance += signal.rgb;
        visibility += signal.a;
    }
    visibility = lightCount > 0u ? visibility / (float)lightCount : 1.0;
    return float4(radiance, visibility);
}

float3 importedLightsRadiance(Surface surface, float3 view, uint2 pixel) {
    return importedLightsSignal(surface, view, pixel).rgb;
}

float3 sampleCosineHemisphere(float3 normal, uint2 pixel, float frameSeed) {
    float3 helper = abs(normal.z) < 0.95 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(helper, normal));
    float3 bitangent = cross(normal, tangent);
    float seed = frameSeed * 37.17 + (float)pixel.x * 0.019 + (float)pixel.y * 0.023;
    float u0 = hash13(normal + float3(seed, 2.1, 5.7));
    float u1 = hash13(normal * 1.37 + float3(4.3, seed, 9.1));
    float r = sqrt(u0);
    float phi = u1 * 2.0 * kPi;
    float z = sqrt(max(0.0, 1.0 - u0));
    return normalize(tangent * (r * cos(phi)) + bitangent * (r * sin(phi)) + normal * z);
}

float3 importedDiffuseBounce(Surface surface, uint2 pixel) {
    if (surface.metalness > 0.5 || surface.roughness < 0.08) {
        return 0.0.xxx;
    }

    float3 bounceDir = sampleCosineHemisphere(surface.normal, pixel, v4Flags.w);
    float hitDistance;
    bool hit = traceSceneRay(surface.worldPos + surface.normal * 0.07, bounceDir, 0.05, max(rightFar.w, 6.0), hitDistance);
    if (!hit) {
        return 0.0.xxx;
    }

    float3 hitPoint = surface.worldPos + surface.normal * 0.07 + bounceDir * hitDistance;
    float2 hitUv;
    float hitCameraZ;
    if (!projectWorldToUv(hitPoint, hitUv, hitCameraZ)) {
        return 0.0.xxx;
    }

    Surface hitSurface;
    if (!readSurface(hitUv, hitSurface) || abs(hitSurface.materialKind - 5.0) < 0.25) {
        return 0.0.xxx;
    }

    float projectedDepth = dot(hitSurface.worldPos - eyeNear.xyz, forwardAspect.xyz);
    if (abs(hitCameraZ - projectedDepth) > max(0.16, hitCameraZ * 0.06)) {
        return 0.0.xxx;
    }

    float3 hitView = normalize(eyeNear.xyz - hitSurface.worldPos);
    float3 hitDirect = importedLightsRadiance(hitSurface, hitView, uint2(hitUv * 8192.0));
    float distanceFalloff = 1.0 / (1.0 + hitDistance * hitDistance * 0.18);
    float receiving = saturate(dot(surface.normal, bounceDir));
    return surface.base * hitSurface.base * hitDirect * receiving * distanceFalloff * 0.55;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width;
    uint height;
    gbufferWorld.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) {
        return;
    }

    float2 uv = (float2(id.xy) + 0.5) / float2(width, height);
    float2 ndc = uv * 2.0 - 1.0 - taaJitter.xy;
    float3 cameraRay = normalize(forwardAspect.xyz + rightFar.xyz * ndc.x * forwardAspect.w * upTanHalf.w + upTanHalf.xyz * -ndc.y * upTanHalf.w);

    Surface surface;
    if (!readSurface(uv, surface)) {
        shadowSignal[id.xy] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionSignal[id.xy] = float4(skyRadiance(cameraRay), 0.0);
        return;
    }
    if (abs(surface.materialKind - 5.0) < 0.25) {
        shadowSignal[id.xy] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionSignal[id.xy] = float4(0.0.xxx, 0.0);
        return;
    }

    float3 view = normalize(eyeNear.xyz - surface.worldPos);
    float3 cameraToSurface = normalize(surface.worldPos - eyeNear.xyz);
    float3 lightDir = normalize(-shadowForwardFar.xyz);

    // Directional shadow visibility — always trace (cost-free if no directional light)
    float visibility = rayTracedVisibility(surface.worldPos, surface.normal, lightDir, id.xy);

    // Local/area light signal (disk-sampled area lights or point lights) + indirect bounce
    float4 localSignal = importedLightsSignal(surface, view, id.xy);
    float3 indirectBounce = importedDiffuseBounce(surface, id.xy);
    float3 localDirect = localSignal.rgb + indirectBounce;
    float ndotv = max(0.04, saturate(dot(surface.normal, view)));
    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(ndotv, f0);
    float smoothness = saturate((0.58 - surface.roughness) / 0.58);
    bool mirrorMaterial = abs(surface.materialKind - 2.0) < 0.25;
    float reflectionWeight = mirrorMaterial ? 1.0 : smoothness * smoothness * lerp(0.025, 0.86, surface.metalness);
    float3 reflection = reflectionWeight > 0.01 ? rayTracedReflection(surface, cameraToSurface, view) : 0.0.xxx;
    float fresnelLuma = dot(fresnel, float3(0.2126, 0.7152, 0.0722));
    shadowSignal[id.xy] = float4(visibility, localDirect);
    reflectionSignal[id.xy] = float4(reflection, saturate(reflectionWeight * fresnelLuma));
}
