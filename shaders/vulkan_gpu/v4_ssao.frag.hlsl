struct FragmentIn {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
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

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(16, 0)]] Texture2D<float4> gbufferNormal;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;

float viewDepth(float3 worldPos) {
    return dot(worldPos - eyeNear.xyz, forwardAspect.xyz);
}

float interleavedGradientNoise(float2 pixel) {
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float main(FragmentIn input) : SV_Target0 {
    float4 worldKind = gbufferWorld.SampleLevel(materialSampler, input.uv, 0.0);
    if (worldKind.w <= 0.5) {
        return 1.0;
    }

    uint width = 1;
    uint height = 1;
    gbufferWorld.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max(width, 1), max(height, 1));

    float3 worldPos = worldKind.xyz;
    float3 normal = normalize(gbufferNormal.SampleLevel(materialSampler, input.uv, 0.0).rgb * 2.0 - 1.0);
    float centerDepth = viewDepth(worldPos);
    float radius = lerp(0.28, 1.10, saturate(centerDepth / 18.0));
    float randomAngle = interleavedGradientNoise(input.position.xy) * 6.2831853;
    float2 cs = float2(cos(randomAngle), sin(randomAngle));

    static const float2 kernel[16] = {
        float2( 0.15,  0.05), float2(-0.20,  0.12), float2( 0.08, -0.24), float2(-0.12, -0.18),
        float2( 0.36,  0.24), float2(-0.42,  0.18), float2( 0.28, -0.44), float2(-0.32, -0.36),
        float2( 0.58,  0.10), float2(-0.64,  0.32), float2( 0.44, -0.68), float2(-0.52, -0.54),
        float2( 0.82,  0.48), float2(-0.86,  0.58), float2( 0.66, -0.92), float2(-0.78, -0.82)
    };

    float occlusion = 0.0;
    float weightSum = 0.0;
    [unroll]
    for (int i = 0; i < 16; ++i) {
        float2 offset = float2(kernel[i].x * cs.x - kernel[i].y * cs.y, kernel[i].x * cs.y + kernel[i].y * cs.x);
        float2 sampleUv = input.uv + offset * texel * 14.0;
        float4 sampleWorld = gbufferWorld.SampleLevel(materialSampler, sampleUv, 0.0);
        if (sampleWorld.w <= 0.5) {
            continue;
        }

        float3 delta = sampleWorld.xyz - worldPos;
        float distanceWeight = saturate(1.0 - length(delta) / radius);
        float normalWeight = saturate(dot(normal, normalize(delta + normal * 0.06)) * 0.70 + 0.30);
        float sampleDepth = viewDepth(sampleWorld.xyz);
        float depthOccluder = sampleDepth < centerDepth - 0.035 ? 1.0 : 0.0;
        occlusion += depthOccluder * distanceWeight * normalWeight;
        weightSum += distanceWeight;
    }

    float ao = 1.0 - saturate(occlusion / max(weightSum, 0.001)) * 0.78;
    return saturate(ao);
}
