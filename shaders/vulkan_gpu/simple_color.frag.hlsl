struct FragmentIn {
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 worldPos : TEXCOORD0;
    [[vk::location(3)]] float2 uv : TEXCOORD1;
    [[vk::location(4)]] float textured : TEXCOORD2;
    [[vk::location(5)]] float roughness : TEXCOORD3;
    [[vk::location(6)]] float metalness : TEXCOORD4;
    [[vk::location(7)]] float materialKind : TEXCOORD5;
    [[vk::location(8)]] float4 tangent : TEXCOORD6;
};

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
};

[[vk::binding(1, 0)]] Texture2D<float4> albedoTexture;
[[vk::binding(2, 0)]] Texture2D<float4> normalTexture;
[[vk::binding(3, 0)]] Texture2D<float4> roughnessTexture;
[[vk::binding(4, 0)]] Texture2D<float4> displacementTexture;
[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(7, 0)]] TextureCube<float4> environmentDiffuseTexture;
[[vk::binding(8, 0)]] TextureCube<float4> environmentSpecularR0Texture;
[[vk::binding(9, 0)]] TextureCube<float4> environmentSpecularR1Texture;
[[vk::binding(10, 0)]] TextureCube<float4> environmentSpecularR2Texture;
[[vk::binding(11, 0)]] TextureCube<float4> environmentSpecularR3Texture;
[[vk::binding(12, 0)]] TextureCube<float4> environmentSpecularR4Texture;
[[vk::binding(13, 0)]] Texture2D<float2> environmentBrdfTexture;
[[vk::binding(14, 0)]] Texture2D<float> directionalShadowTexture;

static const float PI = 3.14159265;

float3 toneMap(float3 hdr) {
    float3 mapped = hdr / (hdr + 1.0.xxx);
    return pow(saturate(mapped), 1.0 / 2.2);
}

float3 skyRadiance(float3 dir) {
    dir = normalize(dir);
    float horizon = saturate(dir.z * 0.5 + 0.5);
    float3 ground = float3(0.18, 0.16, 0.13);
    float3 lowSky = float3(1.10, 0.78, 0.48);
    float3 highSky = float3(0.20, 0.42, 0.88);
    float3 sky = lerp(ground, lerp(lowSky, highSky, horizon), horizon);
    float sun = pow(saturate(dot(dir, normalize(float3(-0.30, -0.62, 0.72)))), 220.0);
    return sky * 1.15 + sun.xxx * float3(7.0, 5.8, 3.8);
}

float3 fresnelSchlick(float cosTheta, float3 f0) {
    return f0 + (1.0.xxx - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float3 sampleDiffuseIrradiance(float3 normal) {
    return environmentDiffuseTexture.SampleLevel(materialSampler, normal, 0.0).rgb;
}

float3 sampleSpecularLevel(float3 reflected, uint level) {
    if (level == 0) return environmentSpecularR0Texture.SampleLevel(materialSampler, reflected, 0.0).rgb;
    if (level == 1) return environmentSpecularR1Texture.SampleLevel(materialSampler, reflected, 0.0).rgb;
    if (level == 2) return environmentSpecularR2Texture.SampleLevel(materialSampler, reflected, 0.0).rgb;
    if (level == 3) return environmentSpecularR3Texture.SampleLevel(materialSampler, reflected, 0.0).rgb;
    return environmentSpecularR4Texture.SampleLevel(materialSampler, reflected, 0.0).rgb;
}

float3 samplePrefilteredSpecular(float3 reflected, float roughness) {
    float level = saturate(roughness) * 4.0;
    uint lo = (uint)floor(level);
    uint hi = min(lo + 1, 4);
    return lerp(sampleSpecularLevel(reflected, lo), sampleSpecularLevel(reflected, hi), frac(level));
}

float2 environmentBrdf(float ndotv, float roughness) {
    return environmentBrdfTexture.SampleLevel(materialSampler, float2(saturate(ndotv), saturate(roughness)), 0.0);
}

float directionalShadow(float3 worldPos, float3 normal) {
    if (v3Flags.x < 0.5) {
        return 1.0;
    }

    float3 offset = worldPos - shadowCenterBias.xyz;
    float extent = max(0.01, shadowRightExtent.w);
    float nearPlane = shadowUpNear.w;
    float farPlane = shadowForwardFar.w;
    float2 uv;
    uv.x = dot(offset, shadowRightExtent.xyz) / extent * 0.5 + 0.5;
    uv.y = -dot(offset, shadowUpNear.xyz) / extent * 0.5 + 0.5;
    float depth = (dot(offset, shadowForwardFar.xyz) - nearPlane) / max(0.01, farPlane - nearPlane);
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || depth <= 0.0 || depth >= 1.0) {
        return 1.0;
    }

    uint shadowWidth = 1;
    uint shadowHeight = 1;
    directionalShadowTexture.GetDimensions(shadowWidth, shadowHeight);
    float2 texel = 1.0 / float2(max(shadowWidth, 1), max(shadowHeight, 1));
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float bias = shadowCenterBias.w + max(0.0007, (1.0 - saturate(dot(normal, lightDir))) * 0.0035);
    float visible = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float closest = directionalShadowTexture.SampleLevel(materialSampler, uv + float2(x, y) * texel, 0.0);
            visible += (depth - bias) <= closest ? 1.0 : 0.0;
        }
    }
    return lerp(0.34, 1.0, visible / 9.0);
}

float3 pointLightRadiance(float3 worldPos, float3 normal, float3 view, float roughness, float3 f0) {
    if (v3Flags.x < 0.5) {
        return 0.0.xxx;
    }
    float3 toLight = pointPosRadius.xyz - worldPos;
    float distanceToLight = length(toLight);
    float3 lightDir = toLight / max(distanceToLight, 0.001);
    float attenuation = saturate(1.0 - distanceToLight / max(pointPosRadius.w, 0.001));
    attenuation *= attenuation;
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float specPower = max(4.0, (1.0 - roughness) * 96.0);
    float specular = pow(saturate(dot(normal, halfVector)), specPower) * (1.0 - roughness * 0.65);
    float3 fresnel = fresnelSchlick(max(0.04, dot(normal, view)), f0);
    return pointColorIntensity.rgb * pointColorIntensity.w * attenuation * (ndotl.xxx + fresnel * specular);
}

float3 spotLightRadiance(float3 worldPos, float3 normal, float3 view, float roughness, float3 f0) {
    if (v3Flags.x < 0.5) {
        return 0.0.xxx;
    }
    float3 toLight = spotPosInner.xyz - worldPos;
    float distanceToLight = length(toLight);
    float3 lightDir = toLight / max(distanceToLight, 0.001);
    float cone = dot(-lightDir, normalize(spotDirOuter.xyz));
    float coneFade = saturate((cone - spotDirOuter.w) / max(0.001, spotPosInner.w - spotDirOuter.w));
    coneFade *= coneFade;
    float attenuation = saturate(1.0 - distanceToLight / 6.5);
    attenuation *= attenuation;
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float specPower = max(4.0, (1.0 - roughness) * 128.0);
    float specular = pow(saturate(dot(normal, halfVector)), specPower) * (1.0 - roughness * 0.55);
    float3 fresnel = fresnelSchlick(max(0.04, dot(normal, view)), f0);
    return spotColorIntensity.rgb * spotColorIntensity.w * coneFade * attenuation * (ndotl.xxx + fresnel * specular);
}

float albedoHeight(float2 uv, float lod) {
    float3 color = displacementTexture.SampleLevel(materialSampler, uv, lod).rgb;
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float mipLevelFromDerivatives(float2 uvDx, float2 uvDy, uint width, uint height) {
    float2 size = float2(max(width, 1), max(height, 1));
    float2 dx = uvDx * size;
    float2 dy = uvDy * size;
    float rho2 = max(dot(dx, dx), dot(dy, dy));
    return max(0.0, 0.5 * log2(max(rho2, 1.0)));
}

float2 parallaxOcclusionMapping(float2 uv, float3 tangentViewDir, float displacementLod) {
    const float heightScale = 0.035;
    const float minLayers = 32.0;
    const float maxLayers = 128.0;
    float numLayers = lerp(maxLayers, minLayers, abs(tangentViewDir.z));
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    float2 deltaUv = tangentViewDir.xy * heightScale / max(0.08, tangentViewDir.z * numLayers);
    deltaUv.y = -deltaUv.y;

    float2 currentUv = uv;
    float currentDepth = displacementTexture.SampleLevel(materialSampler, currentUv, displacementLod).r;
    [loop]
    while (currentDepth > currentLayerDepth && currentLayerDepth < 1.0) {
        currentLayerDepth += layerDepth;
        currentUv -= deltaUv;
        currentDepth = displacementTexture.SampleLevel(materialSampler, currentUv, displacementLod).r;
    }

    float2 previousUv = currentUv + deltaUv;
    float nextDepth = currentDepth - currentLayerDepth;
    float previousDepth = displacementTexture.SampleLevel(materialSampler, previousUv, displacementLod).r - currentLayerDepth + layerDepth;
    float weight = nextDepth / max(1e-5, nextDepth - previousDepth);
    return lerp(currentUv, previousUv, saturate(weight));
}

float4 main(FragmentIn input) : SV_Target0 {
    float3 normal = normalize(input.normal);
    float3 view = normalize(eyeNear.xyz - input.worldPos);
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    float2 uv = input.uv;
    if (input.textured > 0.5) {
        float2 duvdx = ddx(input.uv);
        float2 duvdy = ddy(input.uv);
        float3 tangent = normalize(input.tangent.xyz);
        tangent = normalize(tangent - normal * dot(normal, tangent));
        float3 bitangent = normalize(cross(normal, tangent)) * (input.tangent.w < 0.0 ? -1.0 : 1.0);

        uint displacementWidth = 1;
        uint displacementHeight = 1;
        displacementTexture.GetDimensions(displacementWidth, displacementHeight);
        float displacementLod = mipLevelFromDerivatives(duvdx, duvdy, displacementWidth, displacementHeight);
        float pomFade = saturate(1.0 - displacementLod * 0.28);
        float3 viewTangent = float3(dot(view, tangent), dot(view, bitangent), max(0.22, dot(view, normal)));
        float2 pomUv = parallaxOcclusionMapping(uv, normalize(viewTangent), displacementLod);
        uv = lerp(input.uv, pomUv, pomFade);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            discard;
        }

        uint texWidth = 1;
        uint texHeight = 1;
        albedoTexture.GetDimensions(texWidth, texHeight);
        float2 texel = 1.0 / float2(max(texWidth, 1), max(texHeight, 1));
        float hL = albedoHeight(uv - float2(texel.x, 0.0), displacementLod);
        float hR = albedoHeight(uv + float2(texel.x, 0.0), displacementLod);
        float hD = albedoHeight(uv - float2(0.0, texel.y), displacementLod);
        float hU = albedoHeight(uv + float2(0.0, texel.y), displacementLod);
        float dHdu = (hR - hL) * 3.0 * pomFade;
        float dHdv = (hU - hD) * 3.0 * pomFade;
        float3 tangentNormal = normalTexture.SampleGrad(materialSampler, uv, duvdx, duvdy).xyz * 2.0 - 1.0;
        tangentNormal.y = -tangentNormal.y;
        float3 normalMapped = normalize(tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z);
        float3 heightNormal = normalize(normal - tangent * dHdu - bitangent * dHdv);
        normal = normalize(lerp(heightNormal, normalMapped, 0.82));
    }

    float3 lightDir = v3Flags.x > 0.5 ? normalize(-shadowForwardFar.xyz) : normalize(float3(-0.35, -0.55, 0.76));
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float ndotv = max(0.04, saturate(dot(normal, view)));
    float2 materialUvDx = ddx(input.uv);
    float2 materialUvDy = ddy(input.uv);
    float roughnessMap = roughnessTexture.SampleGrad(materialSampler, uv, materialUvDx, materialUvDy).r;
    float roughness = clamp(lerp(input.roughness, roughnessMap, saturate(input.textured)), 0.03, 1.0);
    float metalness = saturate(input.metalness);
    float materialKind = input.materialKind;

    float3 texel = albedoTexture.SampleGrad(materialSampler, uv, materialUvDx, materialUvDy).rgb;
    float3 base = lerp(saturate(input.color), texel, saturate(input.textured));
    float3 reflected = reflect(-view, normal);
    float3 env = samplePrefilteredSpecular(reflected, roughness);
    float3 mirrorEnv = samplePrefilteredSpecular(reflected, 0.0);
    float3 diffuseIbl = sampleDiffuseIrradiance(normal) * 0.55;

    float specPower = max(2.0, (1.0 - roughness) * 112.0 + 4.0);
    float specTerm = pow(saturate(dot(normal, halfVector)), specPower) * (1.0 - roughness * 0.72);
    float3 f0 = lerp(0.04.xxx, base, metalness);
    float3 fresnel = fresnelSchlick(ndotv, f0);
    float2 brdf = environmentBrdf(ndotv, roughness);
    float rim = pow(1.0 - ndotv, 3.0) * 0.15;
    float shadow = directionalShadow(input.worldPos, normal);
    float3 direct = float3(1.15, 1.05, 0.92) * ndotl * shadow;
    float3 diffuse = base * (1.0 - metalness) * (diffuseIbl + direct);
    float3 specularIbl = env * (fresnel * brdf.x + brdf.y);
    float3 directSpecular = fresnel * specTerm * 0.65 * shadow;
    float3 localLights = (pointLightRadiance(input.worldPos, normal, view, roughness, f0) + spotLightRadiance(input.worldPos, normal, view, roughness, f0)) * base;

    float3 hdr;
    if (materialKind > 1.5 && materialKind < 2.5) {
        hdr = mirrorEnv * 1.20;
    } else if (materialKind > 3.5) {
        hdr = diffuse + specularIbl + directSpecular + localLights + rim.xxx * float3(0.35, 0.45, 0.70);
    } else if (materialKind > 2.5) {
        hdr = base * (diffuseIbl + direct) + localLights;
    } else if (materialKind > 0.5) {
        hdr = mirrorEnv;
    } else {
        hdr = base * (0.24 + 0.80 * ndotl * shadow) + specTerm.xxx * 0.18 * shadow + localLights;
    }

    return float4(toneMap(hdr), 1.0);
}
