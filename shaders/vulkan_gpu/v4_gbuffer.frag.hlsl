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

struct GBufferOut {
    [[vk::location(0)]] float4 albedoRoughness : SV_Target0;
    [[vk::location(1)]] float4 normalMetalness : SV_Target1;
    [[vk::location(2)]] float4 worldKind : SV_Target2;
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

float mipLevelFromDerivatives(float2 uvDx, float2 uvDy, uint width, uint height) {
    float2 size = float2(max(width, 1), max(height, 1));
    float2 dx = uvDx * size;
    float2 dy = uvDy * size;
    float rho2 = max(dot(dx, dx), dot(dy, dy));
    return max(0.0, 0.5 * log2(max(rho2, 1.0)));
}

float2 parallaxOcclusionMapping(float2 uv, float3 tangentViewDir, float displacementLod) {
    const float heightScale = 0.030;
    const float minLayers = 24.0;
    const float maxLayers = 96.0;
    float numLayers = lerp(maxLayers, minLayers, abs(tangentViewDir.z));
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    float2 deltaUv = tangentViewDir.xy * heightScale / max(0.10, tangentViewDir.z * numLayers);
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

float3 srgbToLinear(float3 color) {
    return pow(saturate(color), 2.2);
}

GBufferOut main(FragmentIn input) {
    float3 normal = normalize(input.normal);
    float3 view = normalize(eyeNear.xyz - input.worldPos);
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    float2 uv = input.uv;
    float2 duvdx = ddx(input.uv);
    float2 duvdy = ddy(input.uv);
    const bool hasAlbedoTexture = input.textured > 0.5;
    const bool hasDetailTextures = input.textured > 1.5;
    if (hasDetailTextures) {
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

        float3 tangentNormal = normalTexture.SampleGrad(materialSampler, uv, duvdx, duvdy).xyz * 2.0 - 1.0;
        tangentNormal.y = -tangentNormal.y;
        normal = normalize(tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z);
    }

    float3 texel = srgbToLinear(albedoTexture.SampleGrad(materialSampler, uv, duvdx, duvdy).rgb);
    float3 base = lerp(saturate(input.color), texel, hasAlbedoTexture ? 1.0 : 0.0);
    float roughnessMap = roughnessTexture.SampleGrad(materialSampler, uv, duvdx, duvdy).g;
    float roughness = clamp(lerp(input.roughness, roughnessMap, hasDetailTextures ? 1.0 : 0.0), 0.03, 1.0);

    GBufferOut output;
    output.albedoRoughness = float4(base, roughness);
    output.normalMetalness = float4(normal * 0.5 + 0.5, saturate(input.metalness));
    output.worldKind = float4(input.worldPos, max(1.0, input.materialKind + 1.0));
    return output;
}
