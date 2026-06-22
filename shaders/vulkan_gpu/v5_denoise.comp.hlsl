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
[[vk::binding(22, 0)]] Texture2D<float4> historyColor;
[[vk::binding(23, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> historyOutput : register(u23);
[[vk::binding(24, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> shadowSignal : register(u24);
[[vk::binding(25, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> reflectionSignal : register(u25);
[[vk::binding(26, 0)]] Texture2D<float4> shadowHistory;
[[vk::binding(27, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> shadowHistoryOutput : register(u27);
[[vk::binding(28, 0)]] Texture2D<float4> reflectionHistory;
[[vk::binding(29, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> reflectionHistoryOutput : register(u29);
[[vk::binding(30, 0)]] Texture2D<float4> surfaceHistory;
[[vk::binding(31, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> surfaceHistoryOutput : register(u31);
[[vk::binding(32, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> resolvedColor : register(u32);
[[vk::binding(33, 0), vk::image_format("rg16f")]]
RWTexture2D<float2> motionVector : register(u33);
[[vk::binding(34, 0), vk::image_format("rg16f")]]
RWTexture2D<float2> motionVectorHistory : register(u34);

#include "v5_shared.hlsl"

[[vk::binding(20, 0)]] StructuredBuffer<SceneLight> sceneLights;
[[vk::binding(21, 0)]] RaytracingAccelerationStructure sceneTlas;

float3 skyColorAtUv(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0 - taaJitter.xy;
    float3 cameraRay = normalize(forwardAspect.xyz + rightFar.xyz * ndc.x * forwardAspect.w * upTanHalf.w + upTanHalf.xyz * -ndc.y * upTanHalf.w);
    return toneMap(skyRadiance(cameraRay));
}

bool readSurfacePixel(uint2 pixel, out Surface surface) {
    surface.worldPos = 0.0.xxx;
    surface.normal = float3(0.0, 0.0, 1.0);
    surface.base = 0.0.xxx;
    surface.roughness = 1.0;
    surface.metalness = 0.0;
    surface.materialKind = 0.0;

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

uint2 clampPixel(int2 p, uint width, uint height) {
    return uint2(min(max(p, int2(0, 0)), int2(max(width, 1u) - 1u, max(height, 1u) - 1u)));
}

float depthOf(Surface surface) {
    return dot(surface.worldPos - eyeNear.xyz, forwardAspect.xyz);
}

bool projectPrevWorldToUv(float3 p, out float2 uv, out float previousDepth) {
    float3 rel = p - prevEyeNear.xyz;
    float cameraZ = dot(rel, prevForwardAspect.xyz);
    if (cameraZ <= max(prevEyeNear.w, 0.01)) {
        uv = 0.0.xx;
        previousDepth = cameraZ;
        return false;
    }

    previousDepth = cameraZ;
    float tanHalf = max(prevUpTanHalf.w, 0.001);
    float aspect = max(prevForwardAspect.w, 0.001);
    float2 ndc;
    ndc.x = dot(rel, prevRightFar.xyz) / (cameraZ * aspect * tanHalf);
    ndc.y = -dot(rel, prevUpTanHalf.xyz) / (cameraZ * tanHalf);
    uv = (ndc + prevTaaJitter.xy) * 0.5 + 0.5;
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool validatePreviousSurfaceSample(Surface surface, float4 previousSurface, float projectedDepth) {
    if (previousSurface.a <= 0.0) {
        return false;
    }

    float3 previousNormal = normalize(previousSurface.rgb * 2.0 - 1.0);
    float normalOk = saturate(dot(surface.normal, previousNormal));
    float depthTolerance = max(0.08, projectedDepth * 0.035);
    float depthOk = 1.0 - saturate(abs(previousSurface.a - projectedDepth) / depthTolerance);
    return normalOk > 0.62 && depthOk > 0.12;
}

bool validatePreviousSurface(Surface surface, float2 previousUv, float projectedDepth) {
    return validatePreviousSurfaceSample(surface, surfaceHistory.SampleLevel(materialSampler, previousUv, 0.0), projectedDepth);
}

bool insidePixel(int2 pixel, uint width, uint height) {
    return pixel.x >= 0 && pixel.y >= 0 && pixel.x < (int)width && pixel.y < (int)height;
}

float validatedTapWeight(Surface surface, int2 pixel, uint width, uint height, float projectedDepth) {
    if (!insidePixel(pixel, width, height)) {
        return 0.0;
    }
    float4 previousSurface = surfaceHistory.Load(int3(pixel, 0));
    return validatePreviousSurfaceSample(surface, previousSurface, projectedDepth) ? 1.0 : 0.0;
}

bool validatedPreviousPixel(Surface surface, float2 previousUv, float projectedDepth, out float2 previousPixelFloat, out int2 basePixel, out float2 fracPixel) {
    uint width;
    uint height;
    surfaceHistory.GetDimensions(width, height);
    if (previousUv.x < 0.0 || previousUv.x > 1.0 || previousUv.y < 0.0 || previousUv.y > 1.0 || width == 0 || height == 0) {
        previousPixelFloat = 0.0.xx;
        basePixel = int2(0, 0);
        fracPixel = 0.0.xx;
        return false;
    }
    previousPixelFloat = previousUv * float2(width, height) - 0.5.xx;
    basePixel = int2(floor(previousPixelFloat));
    fracPixel = frac(previousPixelFloat);
    return true;
}

// Catmull-Rom 1D kernel: sharper than bilinear, standard for modern TAA
float catmullRom(float x) {
    float ax = abs(x);
    if (ax < 1.0) return (1.5 * ax - 2.5) * ax * ax + 1.0;
    if (ax < 2.0) return ((-0.5 * ax + 2.5) * ax - 4.0) * ax + 2.0;
    return 0.0;
}

bool sampleValidatedColorHistory(Surface surface, float2 previousUv, float projectedDepth, out float3 color, out float support) {
    uint width;
    uint height;
    historyColor.GetDimensions(width, height);
    float2 previousPixelFloat;
    int2 basePixel;
    float2 fracPixel;
    color = 0.0.xxx;
    support = 0.0;
    if (!validatedPreviousPixel(surface, previousUv, projectedDepth, previousPixelFloat, basePixel, fracPixel)) {
        return false;
    }

    // Catmull-Rom 4x4 filter: 16-tap reconstruction, much sharper than bilinear
    float totalWeight = 0.0;
    [unroll]
    for (int y = -1; y <= 2; ++y) {
        [unroll]
        for (int x = -1; x <= 2; ++x) {
            int2 tap = basePixel + int2(x, y);
            float valid = validatedTapWeight(surface, tap, width, height, projectedDepth);
            if (valid > 0.0) {
                float wx = catmullRom(float(x) - fracPixel.x);
                float wy = catmullRom(float(y) - fracPixel.y);
                float w = wx * wy;
                color += historyColor.Load(int3(tap, 0)).rgb * w;
                totalWeight += w;
                support += w;
            }
        }
    }
    if (totalWeight > 0.0001) {
        color /= totalWeight;
        return true;
    }

    // Fallback: 3x3 box search
    int2 center = int2(round(previousPixelFloat));
    support = 0.0;
    color = 0.0.xxx;
    [unroll]
    for (int fy = -1; fy <= 1; ++fy) {
        [unroll]
        for (int fx = -1; fx <= 1; ++fx) {
            int2 tap = center + int2(fx, fy);
            float valid = validatedTapWeight(surface, tap, width, height, projectedDepth);
            if (valid > 0.0) {
                color += historyColor.Load(int3(tap, 0)).rgb;
                support += 1.0;
            }
        }
    }
    if (support > 0.0001) {
        color /= support;
        support = saturate(support / 9.0);
        return true;
    }
    return false;
}

// RGB → YCoCg: better temporal resolve (chroma artifacts less visible to human eye)
float3 rgbToYCoCg(float3 rgb) {
    float y  = dot(rgb, float3(0.25, 0.50, 0.25));
    float co = dot(rgb, float3(0.50, 0.00, -0.50));
    float cg = dot(rgb, float3(-0.25, 0.50, -0.25));
    return float3(y, co, cg);
}

float3 yCoCgToRgb(float3 ycocg) {
    float t = ycocg.x - ycocg.z;
    return float3(t + ycocg.y, ycocg.x + ycocg.z, t - ycocg.y);
}

bool sampleValidatedShadowHistory(Surface surface, float2 previousUv, float projectedDepth, out float4 signal, out float support) {
    uint width;
    uint height;
    shadowHistory.GetDimensions(width, height);
    float2 previousPixelFloat;
    int2 basePixel;
    float2 fracPixel;
    signal = 0.0.xxxx;
    support = 0.0;
    if (!validatedPreviousPixel(surface, previousUv, projectedDepth, previousPixelFloat, basePixel, fracPixel)) {
        return false;
    }

    float weights[4] = {
        (1.0 - fracPixel.x) * (1.0 - fracPixel.y),
        fracPixel.x * (1.0 - fracPixel.y),
        (1.0 - fracPixel.x) * fracPixel.y,
        fracPixel.x * fracPixel.y
    };
    int2 offsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
    [unroll]
    for (int i = 0; i < 4; ++i) {
        int2 tap = basePixel + offsets[i];
        float valid = validatedTapWeight(surface, tap, width, height, projectedDepth);
        if (valid > 0.0) {
            signal += shadowHistory.Load(int3(tap, 0)) * weights[i];
            support += weights[i];
        }
    }
    if (support > 0.0001) {
        signal /= support;
        return true;
    }
    return false;
}

bool sampleValidatedReflectionHistory(Surface surface, float2 previousUv, float projectedDepth, out float4 signal, out float support) {
    uint width;
    uint height;
    reflectionHistory.GetDimensions(width, height);
    float2 previousPixelFloat;
    int2 basePixel;
    float2 fracPixel;
    signal = 0.0.xxxx;
    support = 0.0;
    if (!validatedPreviousPixel(surface, previousUv, projectedDepth, previousPixelFloat, basePixel, fracPixel)) {
        return false;
    }

    float weights[4] = {
        (1.0 - fracPixel.x) * (1.0 - fracPixel.y),
        fracPixel.x * (1.0 - fracPixel.y),
        (1.0 - fracPixel.x) * fracPixel.y,
        fracPixel.x * fracPixel.y
    };
    int2 offsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
    [unroll]
    for (int i = 0; i < 4; ++i) {
        int2 tap = basePixel + offsets[i];
        float valid = validatedTapWeight(surface, tap, width, height, projectedDepth);
        if (valid > 0.0) {
            signal += reflectionHistory.Load(int3(tap, 0)) * weights[i];
            support += weights[i];
        }
    }
    if (support > 0.0001) {
        signal /= support;
        return true;
    }
    return false;
}

float surfaceEdgeConfidence(uint2 pixel, uint width, uint height, Surface center) {
    float edge = 0.0;
    float centerDepth = depthOf(center);
    int2 centerPixel = int2(pixel);
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }
            Surface sampleSurface;
            if (!readSurfacePixel(clampPixel(centerPixel + int2(x, y), width, height), sampleSurface)) {
                edge = max(edge, 1.0);
                continue;
            }
            float depthEdge = smoothstep(0.04, 0.32, abs(depthOf(sampleSurface) - centerDepth));
            float normalEdge = smoothstep(0.10, 0.46, 1.0 - saturate(dot(center.normal, sampleSurface.normal)));
            edge = max(edge, max(depthEdge, normalEdge));
        }
    }
    return saturate(edge);
}

float bilateralWeight(Surface center, Surface sampleSurface) {
    float normalWeight = pow(saturate(dot(center.normal, sampleSurface.normal)), 24.0);
    float centerDepth = depthOf(center);
    float sampleDepth = depthOf(sampleSurface);
    float depthScale = max(0.35, abs(centerDepth) * 0.02);
    float depthWeight = exp(-abs(centerDepth - sampleDepth) / depthScale);
    float materialWeight = abs(center.materialKind - sampleSurface.materialKind) < 0.25 ? 1.0 : 0.0;
    return normalWeight * depthWeight * materialWeight;
}

float4 temporalShadowAt(uint2 pixel, uint width, uint height, Surface surface) {
    float4 current = shadowSignal[pixel];
    current.r = saturate(current.r);
    current.g = saturate(current.g);
    current.ba = 0.0.xx;
    float4 lo = current;
    float4 hi = current;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float4 v = shadowSignal[clampPixel(int2(pixel) + int2(x, y), width, height)];
            v.r = saturate(v.r);
            v.g = saturate(v.g);
            v.ba = 0.0.xx;
            lo = min(lo, v);
            hi = max(hi, v);
        }
    }
    float2 previousUv;
    float previousDepth;
    float4 previous = current;
    float previousSupport = 0.0;
    bool hasPrevious = projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    hasPrevious = hasPrevious && sampleValidatedShadowHistory(surface, previousUv, previousDepth, previous, previousSupport);
    if (hasPrevious) {
        previous = clamp(previous, lo, hi);
    }
    float historyFrames = max(v4Flags.w, 0.0);
    float historyWeight = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(0.95, historyFrames / (historyFrames + 0.8));
    historyWeight *= saturate(previousSupport);
    // Variance-guided refinement: high-variance shadow/AO signals blend more.
    float2 moments = previous.zw;
    float varMax = max(0.0, moments.y - moments.x * moments.x);
    float varAdjust = saturate(varMax * 18.0);
    historyWeight = lerp(historyWeight, min(0.97, historyWeight + 0.12), varAdjust);
    float disagreement = abs(current.r - previous.r) + abs(current.g - previous.g);
    historyWeight *= 1.0 - saturate(disagreement * 1.8);

    float4 result = lerp(current, previous, historyWeight);
    result.xy = saturate(result.xy);
    float shadowAoLuma = dot(result.xy, float2(0.5, 0.5));
    float2 newMoments = lerp(previous.zw, float2(shadowAoLuma, shadowAoLuma * shadowAoLuma), 0.15);
    result.zw = newMoments;
    return result;
}

float4 temporalReflectionAt(uint2 pixel, uint width, uint height, Surface surface) {
    float4 current = reflectionSignal[pixel];
    float4 lo = current;
    float4 hi = current;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float4 v = reflectionSignal[clampPixel(int2(pixel) + int2(x, y), width, height)];
            lo = min(lo, v);
            hi = max(hi, v);
        }
    }
    float2 previousUv;
    float previousDepth;
    float4 previous = current;
    float previousSupport = 0.0;
    bool hasPrevious = projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    hasPrevious = hasPrevious && sampleValidatedReflectionHistory(surface, previousUv, previousDepth, previous, previousSupport);
    if (hasPrevious) {
        previous = clamp(previous, lo, hi);
    }
    float historyFrames = max(v4Flags.w, 0.0);
    float historyWeight = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(0.93, historyFrames / (historyFrames + 1.2));
    historyWeight *= saturate(previousSupport);
    float lumaCurrent = dot(current.rgb, float3(0.2126, 0.7152, 0.0722));
    float lumaPrevious = dot(previous.rgb, float3(0.2126, 0.7152, 0.0722));
    historyWeight *= 1.0 - saturate(abs(lumaCurrent - lumaPrevious) * 2.0);

    float4 result = lerp(current, previous, historyWeight);
    return float4(max(result.rgb, 0.0.xxx), saturate(result.a));
}

void filterSignals(uint2 pixel, uint width, uint height, Surface center, out float4 shadow, out float4 reflection) {
    float4 shadowSum = 0.0.xxxx;
    float reflectionWeightSum = 0.0;
    float4 reflectionSum = 0.0.xxxx;
    float weightSum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            uint2 samplePixel = clampPixel(int2(pixel) + int2(x, y), width, height);
            Surface sampleSurface;
            if (!readSurfacePixel(samplePixel, sampleSurface)) {
                continue;
            }
            float distance2 = (float)(x * x + y * y);
            float spatialWeight = exp(-distance2 * 0.42);
            float weight = spatialWeight * bilateralWeight(center, sampleSurface);
            if (weight <= 0.0001) {
                continue;
            }
            shadowSum += temporalShadowAt(samplePixel, width, height, sampleSurface) * weight;
            weightSum += weight;

            float4 sampleReflection = temporalReflectionAt(samplePixel, width, height, sampleSurface);
            float reflectionSampleWeight = saturate(sampleReflection.a * 4.0 + (1.0 - sampleSurface.roughness) * 0.35);
            reflectionSum += sampleReflection * weight * reflectionSampleWeight;
            reflectionWeightSum += weight * reflectionSampleWeight;
        }
    }

    shadow = weightSum > 0.0 ? shadowSum / weightSum : temporalShadowAt(pixel, width, height, center);
    bool mirrorMaterial = abs(center.materialKind - 2.0) < 0.25;
    reflection = mirrorMaterial ? temporalReflectionAt(pixel, width, height, center)
                                : (reflectionWeightSum > 0.0 ? reflectionSum / reflectionWeightSum : temporalReflectionAt(pixel, width, height, center));
    shadow.r = saturate(shadow.r);
    shadow.gba = max(shadow.gba, 0.0.xxx);
    reflection = max(reflection, 0.0.xxxx);
}

// À-trous spatial iteration: filter with given step size, reading from previous-filtered shadow/reflection.
// The input signals (shadowIn/reflectionIn) are the step-(N-1) output; this function produces step-N output.
void atrousIteration(uint2 pixel, uint width, uint height, Surface center, float4 shadowIn, float4 reflectionIn,
                     out float4 shadowOut, out float4 reflectionOut, int stepSize) {
    float4 shadowSum = shadowIn;
    float reflectionWeightSum = 1.0;
    float4 reflectionSum = reflectionIn;
    float weightSum = 1.0;

    // Mirror or near-mirror surfaces keep sharp reflections — skip spatial filtering
    float centerSmoothness = saturate((0.58 - center.roughness) / 0.58);
    bool mirrorMaterial = abs(center.materialKind - 2.0) < 0.25;
    float reflectionFilterStrength = mirrorMaterial ? 0.0 : saturate(1.0 - centerSmoothness * 0.92);

    // 5×5 kernel with stride = stepSize, giving effective radius ≈ stepSize * 2
    [unroll]
    for (int y = -2; y <= 2; ++y) {
        [unroll]
        for (int x = -2; x <= 2; ++x) {
            if (x == 0 && y == 0) continue;
            uint2 samplePixel = clampPixel(int2(pixel) + int2(x, y) * stepSize, width, height);
            Surface sampleSurface;
            if (!readSurfacePixel(samplePixel, sampleSurface)) continue;

            float distance2 = (float)(x * x + y * y);
            float spatialWeight = exp(-distance2 * 0.18);
            float weight = spatialWeight * bilateralWeight(center, sampleSurface);
            if (weight <= 0.0005) continue;

            float4 sampleShadow = temporalShadowAt(samplePixel, width, height, sampleSurface);
            shadowSum += sampleShadow * weight;
            weightSum += weight;

            float4 sampleReflection = temporalReflectionAt(samplePixel, width, height, sampleSurface);
            float reflectionSampleWeight = saturate(sampleReflection.a * 3.0 + (1.0 - sampleSurface.roughness) * 0.25);
            reflectionSum += sampleReflection * weight * reflectionSampleWeight;
            reflectionWeightSum += weight * reflectionSampleWeight;
        }
    }

    shadowOut = weightSum > 0.0 ? shadowSum / weightSum : shadowIn;
    shadowOut.r = saturate(shadowOut.r);
    shadowOut.gba = max(shadowOut.gba, 0.0.xxx);

    // Blend spatially-filtered reflection with input based on roughness: mirrors stay sharp
    float4 spatialReflection = reflectionWeightSum > 0.0 ? reflectionSum / reflectionWeightSum : reflectionIn;
    spatialReflection = max(spatialReflection, 0.0.xxxx);
    reflectionOut = lerp(reflectionIn, spatialReflection, reflectionFilterStrength);
}

float traceShadowRay(float3 origin, float3 normal, float3 rayDir, float maxDistance);
float4 pbrLocalLightSignal(Surface surface, float3 view, SceneLight light, uint2 pixel);

float3 importedRasterDirect(Surface surface, float3 view, uint2 pixel) {
    uint lightCount = min((uint)max(0.0, v4Flags.y), 128u);
    float3 direct = 0.0.xxx;
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        direct += pbrLocalLightSignal(surface, view, sceneLights[i], pixel).rgb;
    }
    return direct;
}

float3 composeLinear(Surface surface, float4 shadow, float3 reflection, uint2 pixel) {
    float3 view = normalize(eyeNear.xyz - surface.worldPos);
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float raytracedShadow = saturate(shadow.r);
    float raytracedAo = 1.0;  // AO disabled — RT pipeline no longer traces AO rays
    float3 direct = importedRasterDirect(surface, view, pixel);
    direct += pbrDirectional(surface, view, lightDir, float3(1.10, 1.04, 0.92), 12.0, raytracedShadow);
    float ndotv = max(0.04, saturate(dot(surface.normal, view)));
    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(ndotv, f0);
    float smoothness = saturate((0.58 - surface.roughness) / 0.58);
    bool mirrorMaterial = abs(surface.materialKind - 2.0) < 0.25;
    float reflectionWeight = smoothness * smoothness * lerp(0.025, 0.86, surface.metalness);
    float replacementWeight = mirrorMaterial ? 1.0 : saturate(surface.metalness * smoothness * smoothness);
    float3 ambient = mirrorMaterial ? 0.0.xxx : surface.base * (raytracedAo / kPi);
    float3 reflectedSpecular = reflection * (mirrorMaterial ? 1.0 : raytracedShadow);
    float3 dielectricReflection = reflection * fresnel * reflectionWeight * (1.0 - replacementWeight);
    return ambient + direct * (1.0 - replacementWeight) + reflectedSpecular * replacementWeight + dielectricReflection;
}

float3 cheapToneAt(uint2 pixel, uint width, uint height) {
    Surface surface;
    if (!readSurfacePixel(pixel, surface)) {
        float2 uv = (float2(pixel) + 0.5) / float2(max(width, 1u), max(height, 1u));
        return skyColorAtUv(uv);
    }
    if (abs(surface.materialKind - 5.0) < 0.25) {
        return toneMap(surface.base * 18.0);
    }
    float4 rawShadow = shadowSignal[pixel];
    rawShadow.r = saturate(rawShadow.r);
    rawShadow.g = saturate(rawShadow.g);
    return toneMap(composeLinear(surface, rawShadow, max(reflectionSignal[pixel].rgb, 0.0.xxx), pixel));
}

float edgeStrength(uint2 pixel, uint width, uint height) {
    Surface center;
    if (!readSurfacePixel(pixel, center)) {
        return 0.0;
    }
    int2 centerPixel = int2(pixel);
    float centerDepth = depthOf(center);
    float edge = 0.0;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        int2 offset = i == 0 ? int2(1, 0) : (i == 1 ? int2(-1, 0) : (i == 2 ? int2(0, 1) : int2(0, -1)));
        uint2 samplePixel = clampPixel(centerPixel + offset, width, height);
        Surface sampleSurface;
        if (!readSurfacePixel(samplePixel, sampleSurface)) {
            edge = max(edge, 1.0);
            continue;
        }
        float depthEdge = smoothstep(0.05, 0.45, abs(depthOf(sampleSurface) - centerDepth));
        float normalEdge = smoothstep(0.18, 0.65, 1.0 - saturate(dot(center.normal, sampleSurface.normal)));
        float materialEdge = abs(sampleSurface.materialKind - center.materialKind) > 0.25 ? 1.0 : 0.0;
        edge = max(edge, max(materialEdge, max(depthEdge, normalEdge)));
    }
    return edge;
}

float3 applyEdgeAntialias(uint2 pixel, uint width, uint height, float3 color) {
    float edge = edgeStrength(pixel, width, height);
    if (edge <= 0.001) {
        return color;
    }
    int2 centerPixel = int2(pixel);
    float3 n = cheapToneAt(clampPixel(centerPixel + int2(0, -1), width, height), width, height);
    float3 s = cheapToneAt(clampPixel(centerPixel + int2(0, 1), width, height), width, height);
    float3 e = cheapToneAt(clampPixel(centerPixel + int2(1, 0), width, height), width, height);
    float3 w = cheapToneAt(clampPixel(centerPixel + int2(-1, 0), width, height), width, height);
    float3 ne = cheapToneAt(clampPixel(centerPixel + int2(1, -1), width, height), width, height);
    float3 nw = cheapToneAt(clampPixel(centerPixel + int2(-1, -1), width, height), width, height);
    float3 se = cheapToneAt(clampPixel(centerPixel + int2(1, 1), width, height), width, height);
    float3 sw = cheapToneAt(clampPixel(centerPixel + int2(-1, 1), width, height), width, height);

    const float3 kLuma = float3(0.2126, 0.7152, 0.0722);
    float lc = dot(color, kLuma);
    float ln = dot(n, kLuma);
    float ls = dot(s, kLuma);
    float le = dot(e, kLuma);
    float lw = dot(w, kLuma);
    float lne = dot(ne, kLuma);
    float lnw = dot(nw, kLuma);
    float lse = dot(se, kLuma);
    float lsw = dot(sw, kLuma);
    float lmin = min(lc, min(min(ln, ls), min(min(le, lw), min(min(lne, lnw), min(lse, lsw)))));
    float lmax = max(lc, max(max(ln, ls), max(max(le, lw), max(max(lne, lnw), max(lse, lsw)))));
    float contrast = lmax - lmin;
    if (contrast < max(0.025, lmax * 0.10)) {
        return color;
    }

    float horizontal = abs(lnw + 2.0 * ln + lne - lsw - 2.0 * ls - lse);
    float vertical = abs(lnw + 2.0 * lw + lsw - lne - 2.0 * le - lse);
    float diagA = abs(lnw + lc + lse - lne - lc - lsw);
    float diagB = abs(lne + lc + lsw - lnw - lc - lse);
    float3 alongEdge = horizontal >= vertical ? (w + e) * 0.5 : (n + s) * 0.5;
    float3 diagonalEdge = diagA > diagB ? (ne + sw) * 0.5 : (nw + se) * 0.5;
    float diagonalWeight = saturate(abs(diagA - diagB) * 3.0);
    float3 resolved = lerp(alongEdge, diagonalEdge, diagonalWeight * 0.35);
    float blend = saturate(edge * 0.62 + contrast * 1.15);
    return lerp(color, resolved, min(0.72, blend));
}

float3 applyAdaptiveSharpen(uint2 pixel, uint width, uint height, float3 color, float edge) {
    int2 centerPixel = int2(pixel);
    float3 neighborAverage =
        cheapToneAt(clampPixel(centerPixel + int2(1, 0), width, height), width, height) +
        cheapToneAt(clampPixel(centerPixel + int2(-1, 0), width, height), width, height) +
        cheapToneAt(clampPixel(centerPixel + int2(0, 1), width, height), width, height) +
        cheapToneAt(clampPixel(centerPixel + int2(0, -1), width, height), width, height);
    neighborAverage *= 0.25;
    float strength = lerp(0.035, 0.0, edge);
    return saturate(color + (color - neighborAverage) * strength);
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
    float normalSide = dot(normal, rayDir) >= 0.0 ? 1.0 : -1.0;
    bool hit = traceSceneRay(origin + normal * (0.05 * normalSide) + rayDir * 0.01, rayDir, 0.035, maxDistance, hitDistance);
    return hit ? 0.0 : 1.0;
}

float4 pbrLocalLightSignal(Surface surface, float3 view, SceneLight light, uint2 pixel) {
    float3 lightPosition = light.positionRadius.xyz;
    if (light.normalArea.w > 0.0001) {
        float3 lightNormal = normalize(light.normalArea.xyz);
        float3 helper = abs(lightNormal.z) < 0.95 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
        float3 tangent = normalize(cross(helper, lightNormal));
        float3 bitangent = cross(lightNormal, tangent);
        float diskRadius = max(light.positionRadius.w, sqrt(light.normalArea.w / kPi));

        static const float2 areaTaps[4] = {
            float2(0.0, 0.0),
            float2(0.58, 0.0),
            float2(-0.34, 0.47),
            float2(0.18, -0.55),
        };

        float3 radianceSum = 0.0.xxx;
        float visibilitySum = 0.0;
        [unroll]
        for (int tap = 0; tap < 4; ++tap) {
            float2 disk = areaTaps[tap];
            float3 samplePosition = lightPosition + (tangent * disk.x + bitangent * disk.y) * diskRadius;
            float3 toSample = samplePosition - surface.worldPos;
            float distanceToSample = length(toSample);
            float3 sampleDir = toSample / max(distanceToSample, 0.001);
            float emissionCos = saturate(dot(lightNormal, -sampleDir));
            float ndotl = saturate(dot(surface.normal, sampleDir));
            if (emissionCos <= 0.0 || ndotl <= 0.0) {
                continue;
            }

            float visibility = traceShadowRay(surface.worldPos, surface.normal, sampleDir, max(0.04, distanceToSample - 0.025));
            float attenuation = emissionCos * light.normalArea.w / max(distanceToSample * distanceToSample, 0.05);
            radianceSum += pbrDirectional(surface, view, sampleDir, saturate(light.colorIntensity.rgb), light.colorIntensity.w * attenuation / kPi, visibility);
            visibilitySum += visibility;
        }
        return float4(radianceSum * 0.25, visibilitySum * 0.25);
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

void writeSurfaceHistory(uint2 pixel, Surface surface, bool hasSurface) {
    if (!hasSurface) {
        surfaceHistoryOutput[pixel] = float4(0.5, 0.5, 1.0, 0.0);
        return;
    }

    float viewDepth = max(0.0, depthOf(surface));
    surfaceHistoryOutput[pixel] = float4(saturate(surface.normal * 0.5 + 0.5), viewDepth);
}

void writeAccumulatedColor(uint2 pixel, uint width, uint height, float3 currentColor, Surface surface, bool hasSurface) {
    int2 centerPixel = int2(pixel);
    float3 neighborhoodMin = currentColor;
    float3 neighborhoodMax = currentColor;
    float3 neighborhoodSum = currentColor;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }
            float3 sampleColor = cheapToneAt(clampPixel(centerPixel + int2(x, y), width, height), width, height);
            neighborhoodMin = min(neighborhoodMin, sampleColor);
            neighborhoodMax = max(neighborhoodMax, sampleColor);
            neighborhoodSum += sampleColor;
        }
    }
    float3 neighborhoodMean = neighborhoodSum / 9.0;
    // YCoCg temporal resolve: luma resolves faster, chroma more conservative
    float3 currentYCoCg = rgbToYCoCg(currentColor);
    float3 clipPadding = max(0.018.xxx, abs(neighborhoodMean - currentColor) * 0.65 + 0.022.xxx);
    neighborhoodMin -= clipPadding;
    neighborhoodMax += clipPadding;
    float2 previousUv;
    float previousDepth;
    bool hasPrevious = hasSurface && projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    float3 previousColor = currentColor;
    float previousSupport = 0.0;
    if (hasPrevious) {
        hasPrevious = sampleValidatedColorHistory(surface, previousUv, previousDepth, previousColor, previousSupport);
    }
    if (hasPrevious) {
        previousColor = clamp(previousColor, neighborhoodMin, neighborhoodMax);
    }
    float3 previousYCoCg = rgbToYCoCg(previousColor);

    float historyFrames = max(v4Flags.w, 0.0);
    float edge = hasSurface ? surfaceEdgeConfidence(pixel, width, height, surface) : 0.0;
    float baseHistoryWeight = lerp(0.94, 0.975, edge);
    float historyWeightLuma = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(baseHistoryWeight, historyFrames / (historyFrames + 0.45));
    historyWeightLuma *= saturate(previousSupport);

    // Variance-guided alpha (SVGF-style): noisy pixels blend more aggressively
    float lumaVariance = abs(currentYCoCg.x - previousYCoCg.x);
    lumaVariance *= lumaVariance;
    float sigma = max(0.002, sqrt(lumaVariance));
    float varianceBoost = saturate(sigma / (sigma + 0.04));
    historyWeightLuma = max(historyWeightLuma, varianceBoost * 0.92);

    float lumaDisagreement = abs(currentYCoCg.x - previousYCoCg.x);
    historyWeightLuma *= 1.0 - saturate(lumaDisagreement * lerp(3.5, 1.5, edge));

    // Chroma: slower convergence = less ghosting on color edges
    float historyWeightChroma = historyWeightLuma * 0.85;
    float chromaDisagreement = abs(currentYCoCg.y - previousYCoCg.y) + abs(currentYCoCg.z - previousYCoCg.z);
    historyWeightChroma *= 1.0 - saturate(chromaDisagreement * 5.0);

    float3 accumulatedYCoCg;
    accumulatedYCoCg.x = lerp(currentYCoCg.x, previousYCoCg.x, historyWeightLuma);
    accumulatedYCoCg.y = lerp(currentYCoCg.y, previousYCoCg.y, historyWeightChroma);
    accumulatedYCoCg.z = lerp(currentYCoCg.z, previousYCoCg.z, historyWeightChroma);
    float3 accumulated = yCoCgToRgb(accumulatedYCoCg);
    float viewDepth = hasSurface ? max(0.0, depthOf(surface)) : 0.0;
    historyOutput[pixel] = float4(accumulated, viewDepth);
    float3 antialiased = applyEdgeAntialias(pixel, width, height, accumulated);
    resolvedColor[pixel] = float4(applyAdaptiveSharpen(pixel, width, height, antialiased, edge), 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width;
    uint height;
    resolvedColor.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) {
        return;
    }

    uint2 pixel = id.xy;
    Surface surface;
    if (!readSurfacePixel(pixel, surface)) {
        float2 uv = (float2(pixel) + 0.5) / float2(width, height);
        shadowHistoryOutput[pixel] = float4(1.0, 1.0, 0.0, 0.0);
        reflectionHistoryOutput[pixel] = float4(0.0.xxxx);
        writeSurfaceHistory(pixel, surface, false);
        float3 background = skyColorAtUv(uv);
        writeAccumulatedColor(pixel, width, height, background, surface, false);
        return;
    }

    if (abs(surface.materialKind - 5.0) < 0.25) {
        shadowHistoryOutput[pixel] = float4(1.0, 1.0, 0.0, 0.0);
        reflectionHistoryOutput[pixel] = float4(0.0.xxxx);
        writeSurfaceHistory(pixel, surface, true);
        writeAccumulatedColor(pixel, width, height, toneMap(surface.base * 18.0), surface, true);
        return;
    }

    // Motion vector: current UV → previous UV for FSR 2.0 / temporal resolve
    float2 currentUv = (float2(pixel) + 0.5) / float2(width, height);
    float2 prevUv;
    float prevDepth;
    if (projectPrevWorldToUv(surface.worldPos, prevUv, prevDepth)) {
        motionVector[pixel] = currentUv - prevUv;
    } else {
        motionVector[pixel] = float2(0.0, 0.0);
    }

    float4 filteredShadow;
    float4 filteredReflection;
    filterSignals(pixel, width, height, surface, filteredShadow, filteredReflection);

    // À-trous multi-scale spatial refinement (step=2, step=4)
    float4 shadowStep2, reflectionStep2;
    atrousIteration(pixel, width, height, surface, filteredShadow, filteredReflection,
                    shadowStep2, reflectionStep2, 2);

    float4 shadowStep4, reflectionStep4;
    atrousIteration(pixel, width, height, surface, shadowStep2, reflectionStep2,
                    shadowStep4, reflectionStep4, 4);

    shadowHistoryOutput[pixel] = shadowStep4;
    reflectionHistoryOutput[pixel] = float4(reflectionStep4.rgb, saturate(reflectionStep4.a));
    writeSurfaceHistory(pixel, surface, true);
    writeAccumulatedColor(pixel, width, height, toneMap(composeLinear(surface, shadowStep4, reflectionStep4.rgb, pixel)), surface, true);
}
