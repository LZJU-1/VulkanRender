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

static const float kPi = 3.14159265;

struct Surface {
    float3 worldPos;
    float3 normal;
    float3 base;
    float roughness;
    float metalness;
    float materialKind;
};

struct SceneLight {
    float4 positionRadius;
    float4 colorIntensity;
};

[[vk::binding(20, 0)]] StructuredBuffer<SceneLight> sceneLights;

float3 skyRadiance(float3 dir) {
    dir = normalize(dir);
    float horizon = saturate(dir.z * 0.5 + 0.5);
    float3 ground = float3(0.18, 0.17, 0.15);
    float3 lowSky = float3(1.04, 0.82, 0.56);
    float3 highSky = float3(0.18, 0.38, 0.78);
    float sun = pow(saturate(dot(dir, normalize(float3(-0.35, -0.48, 0.80)))), 360.0);
    return lerp(ground, lerp(lowSky, highSky, horizon), horizon) + sun.xxx * float3(7.0, 5.5, 3.4);
}

float3 toneMap(float3 color) {
    color = max(color, 0.0.xxx);
    color = color / (color + 1.0.xxx);
    return pow(saturate(color), 1.0 / 2.2);
}

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

float3 fresnelSchlick(float cosTheta, float3 f0) {
    return f0 + (1.0.xxx - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float3 pbrDirectional(Surface surface, float3 view, float3 lightDir, float3 lightColor, float lightIntensity, float visibility) {
    float ndotl = saturate(dot(surface.normal, lightDir));
    float ndotv = max(0.04, saturate(dot(surface.normal, view)));
    if (ndotl <= 0.0) {
        return 0.0.xxx;
    }

    float3 halfVector = normalize(lightDir + view);
    float ndoth = saturate(dot(surface.normal, halfVector));
    float alpha = max(0.035, surface.roughness * surface.roughness);
    float alpha2 = alpha * alpha;
    float denom = ndoth * ndoth * (alpha2 - 1.0) + 1.0;
    float distribution = alpha2 / max(0.001, kPi * denom * denom);
    float k = (surface.roughness + 1.0) * (surface.roughness + 1.0) * 0.125;
    float geometry = ndotl / max(0.001, ndotl * (1.0 - k) + k);
    geometry *= ndotv / max(0.001, ndotv * (1.0 - k) + k);
    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(saturate(dot(halfVector, view)), f0);
    float3 specular = distribution * geometry * fresnel / max(0.001, 4.0 * ndotl * ndotv);
    float3 diffuse = surface.base * (1.0 - surface.metalness) * (1.0.xxx - fresnel) / kPi;
    return lightColor * lightIntensity * visibility * ndotl * (diffuse + specular);
}

float3 importedLightsRadiance(Surface surface, float3 view) {
    uint lightCount = min((uint)max(0.0, v4Flags.y), 128u);
    float3 radiance = 0.0.xxx;
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        SceneLight light = sceneLights[i];
        float3 toLight = light.positionRadius.xyz - surface.worldPos;
        float distanceToLight = length(toLight);
        float radius = max(0.25, light.positionRadius.w);
        float3 lightDir = toLight / max(distanceToLight, 0.001);
        if (distanceToLight >= radius || dot(surface.normal, lightDir) <= 0.0) {
            continue;
        }
        float falloff = saturate(1.0 - distanceToLight / radius);
        falloff = falloff * falloff * (3.0 - 2.0 * falloff);
        radiance += pbrDirectional(surface, view, lightDir, saturate(light.colorIntensity.rgb), light.colorIntensity.w * falloff, 1.0);
    }
    return radiance;
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

bool validatePreviousSurface(Surface surface, float2 previousUv, float projectedDepth) {
    float4 previousSurface = surfaceHistory.SampleLevel(materialSampler, previousUv, 0.0);
    if (previousSurface.a <= 0.0) {
        return false;
    }

    float3 previousNormal = normalize(previousSurface.rgb * 2.0 - 1.0);
    float normalOk = saturate(dot(surface.normal, previousNormal));
    float depthTolerance = max(0.08, projectedDepth * 0.035);
    float depthOk = 1.0 - saturate(abs(previousSurface.a - projectedDepth) / depthTolerance);
    return normalOk > 0.62 && depthOk > 0.12;
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
    current.gba = max(current.gba, 0.0.xxx);
    float4 lo = current;
    float4 hi = current;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float4 v = shadowSignal[clampPixel(int2(pixel) + int2(x, y), width, height)];
            v.r = saturate(v.r);
            v.gba = max(v.gba, 0.0.xxx);
            lo = min(lo, v);
            hi = max(hi, v);
        }
    }
    float2 previousUv;
    float previousDepth;
    float4 previous = current;
    bool hasPrevious = projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    hasPrevious = hasPrevious && validatePreviousSurface(surface, previousUv, previousDepth);
    if (hasPrevious) {
        previous = clamp(shadowHistory.SampleLevel(materialSampler, previousUv, 0.0), lo, hi);
    }
    float historyFrames = max(v4Flags.w, 0.0);
    float historyWeight = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(0.96, historyFrames / (historyFrames + 1.0));
    float localCurrent = dot(current.gba, float3(0.2126, 0.7152, 0.0722));
    float localPrevious = dot(previous.gba, float3(0.2126, 0.7152, 0.0722));
    float disagreement = abs(current.r - previous.r) + abs(localCurrent - localPrevious) * 0.25;
    historyWeight *= 1.0 - saturate(disagreement * 1.8);
    return lerp(current, previous, historyWeight);
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
    bool hasPrevious = projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    hasPrevious = hasPrevious && validatePreviousSurface(surface, previousUv, previousDepth);
    if (hasPrevious) {
        previous = clamp(reflectionHistory.SampleLevel(materialSampler, previousUv, 0.0), lo, hi);
    }
    float historyFrames = max(v4Flags.w, 0.0);
    float historyWeight = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(0.94, historyFrames / (historyFrames + 2.0));
    float lumaCurrent = dot(current.rgb, float3(0.2126, 0.7152, 0.0722));
    float lumaPrevious = dot(previous.rgb, float3(0.2126, 0.7152, 0.0722));
    historyWeight *= 1.0 - saturate(abs(lumaCurrent - lumaPrevious) * 2.0);
    return lerp(current, previous, historyWeight);
}

void filterSignals(uint2 pixel, uint width, uint height, Surface center, out float4 shadow, out float4 reflection) {
    float4 shadowSum = 0.0.xxxx;
    float reflectionWeightSum = 0.0;
    float4 reflectionSum = 0.0.xxxx;
    float weightSum = 0.0;

    [unroll]
    for (int y = -2; y <= 2; ++y) {
        [unroll]
        for (int x = -2; x <= 2; ++x) {
            uint2 samplePixel = clampPixel(int2(pixel) + int2(x, y), width, height);
            Surface sampleSurface;
            if (!readSurfacePixel(samplePixel, sampleSurface)) {
                continue;
            }
            float distance2 = (float)(x * x + y * y);
            float spatialWeight = exp(-distance2 * 0.28);
            float weight = spatialWeight * bilateralWeight(center, sampleSurface);
            if (weight <= 0.0001) {
                continue;
            }
            shadowSum += temporalShadowAt(samplePixel, width, height, sampleSurface) * weight;
            weightSum += weight;

            float4 sampleReflection = temporalReflectionAt(samplePixel, width, height, sampleSurface);
            float reflectionGuide = saturate(sampleReflection.a * 4.0 + (1.0 - sampleSurface.roughness) * 0.35);
            reflectionSum += sampleReflection * weight * reflectionGuide;
            reflectionWeightSum += weight * reflectionGuide;
        }
    }

    shadow = weightSum > 0.0 ? shadowSum / weightSum : temporalShadowAt(pixel, width, height, center);
    reflection = reflectionWeightSum > 0.0 ? reflectionSum / reflectionWeightSum : temporalReflectionAt(pixel, width, height, center);
    shadow.r = saturate(shadow.r);
    shadow.gba = max(shadow.gba, 0.0.xxx);
    reflection = max(reflection, 0.0.xxxx);
}

float3 composeLinear(Surface surface, float4 shadow, float3 reflection) {
    float3 view = normalize(eyeNear.xyz - surface.worldPos);
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float3 direct = pbrDirectional(surface, view, lightDir, float3(1.10, 1.04, 0.92), 3.15, shadow.r);
    direct += shadow.gba;
    float ndotv = max(0.04, saturate(dot(surface.normal, view)));
    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(ndotv, f0);
    float smoothness = saturate((0.58 - surface.roughness) / 0.58);
    bool mirrorMaterial = abs(surface.materialKind - 2.0) < 0.25;
    float reflectionWeight = mirrorMaterial ? 1.0 : smoothness * smoothness * lerp(0.025, 0.86, surface.metalness);
    float3 ambient = mirrorMaterial ? 0.0.xxx : surface.base * 0.24;
    return ambient + direct + reflection * fresnel * reflectionWeight;
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
    rawShadow.gba = max(rawShadow.gba, 0.0.xxx);
    return toneMap(composeLinear(surface, rawShadow, max(reflectionSignal[pixel].rgb, 0.0.xxx)));
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
    float strength = lerp(0.10, 0.0, edge);
    return saturate(color + (color - neighborAverage) * strength);
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
    float3 clipPadding = max(0.012.xxx, abs(neighborhoodMean - currentColor) * 0.55 + 0.018.xxx);
    neighborhoodMin -= clipPadding;
    neighborhoodMax += clipPadding;
    float2 previousUv;
    float previousDepth;
    bool hasPrevious = hasSurface && projectPrevWorldToUv(surface.worldPos, previousUv, previousDepth);
    hasPrevious = hasPrevious && validatePreviousSurface(surface, previousUv, previousDepth);
    float3 previousColor = currentColor;
    if (hasPrevious) {
        previousColor = clamp(historyColor.SampleLevel(materialSampler, previousUv, 0.0).rgb, neighborhoodMin, neighborhoodMax);
    }
    float lumaCurrent = dot(currentColor, float3(0.2126, 0.7152, 0.0722));
    float lumaPrevious = dot(previousColor, float3(0.2126, 0.7152, 0.0722));
    float historyFrames = max(v4Flags.w, 0.0);
    float edge = hasSurface ? surfaceEdgeConfidence(pixel, width, height, surface) : 0.0;
    float baseHistoryWeight = lerp(0.91, 0.965, edge);
    float historyWeight = (historyFrames < 1.0 || !hasPrevious) ? 0.0 : min(baseHistoryWeight, historyFrames / (historyFrames + 0.65));
    float lumaDisagreement = abs(lumaCurrent - lumaPrevious);
    historyWeight *= 1.0 - saturate(lumaDisagreement * lerp(4.0, 1.85, edge));
    float3 accumulated = lerp(currentColor, previousColor, historyWeight);
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
        shadowHistoryOutput[pixel] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionHistoryOutput[pixel] = float4(0.0.xxxx);
        writeSurfaceHistory(pixel, surface, false);
        writeAccumulatedColor(pixel, width, height, skyColorAtUv(uv), surface, false);
        return;
    }

    if (abs(surface.materialKind - 5.0) < 0.25) {
        shadowHistoryOutput[pixel] = float4(1.0, 0.0, 0.0, 0.0);
        reflectionHistoryOutput[pixel] = float4(0.0.xxxx);
        writeSurfaceHistory(pixel, surface, true);
        writeAccumulatedColor(pixel, width, height, toneMap(surface.base * 18.0), surface, true);
        return;
    }

    float4 filteredShadow;
    float4 filteredReflection;
    filterSignals(pixel, width, height, surface, filteredShadow, filteredReflection);
    shadowHistoryOutput[pixel] = filteredShadow;
    reflectionHistoryOutput[pixel] = float4(filteredReflection.rgb, saturate(filteredReflection.a));
    writeSurfaceHistory(pixel, surface, true);
    writeAccumulatedColor(pixel, width, height, toneMap(composeLinear(surface, filteredShadow, filteredReflection.rgb)), surface, true);
}
