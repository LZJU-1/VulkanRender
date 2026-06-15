struct FragmentIn {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;
[[vk::binding(18, 0)]] Texture2D<float> ssaoRaw;

float main(FragmentIn input) : SV_Target0 {
    float4 centerWorld = gbufferWorld.SampleLevel(materialSampler, input.uv, 0.0);
    if (centerWorld.w <= 0.5) {
        return 1.0;
    }

    uint width = 1;
    uint height = 1;
    ssaoRaw.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max(width, 1), max(height, 1));

    float result = 0.0;
    float weightSum = 0.0;
    [unroll]
    for (int y = -2; y <= 2; ++y) {
        [unroll]
        for (int x = -2; x <= 2; ++x) {
            float2 sampleUv = input.uv + float2(x, y) * texel;
            float4 sampleWorld = gbufferWorld.SampleLevel(materialSampler, sampleUv, 0.0);
            float depthWeight = sampleWorld.w > 0.5 ? saturate(1.0 - length(sampleWorld.xyz - centerWorld.xyz) * 2.2) : 0.0;
            float spatial = 1.0 / (1.0 + dot(float2(x, y), float2(x, y)));
            float weight = spatial * max(depthWeight, (x == 0 && y == 0) ? 1.0 : 0.0);
            result += ssaoRaw.SampleLevel(materialSampler, sampleUv, 0.0) * weight;
            weightSum += weight;
        }
    }

    return saturate(result / max(weightSum, 0.001));
}
