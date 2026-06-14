struct FragmentIn {
    [[vk::location(0)]] float2 ndc : TEXCOORD0;
};

cbuffer Camera : register(b0) {
    float4 eyeNear;
    float4 rightFar;
    float4 upTanHalf;
    float4 forwardAspect;
};

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(6, 0)]] TextureCube<float4> environmentBackgroundTexture;

float3 toneMap(float3 hdr) {
    float3 mapped = hdr / (hdr + 1.0.xxx);
    return pow(saturate(mapped), 1.0 / 2.2);
}

float4 main(FragmentIn input) : SV_Target0 {
    float2 ndc = input.ndc;
    float3 dir = normalize(
        forwardAspect.xyz
        + rightFar.xyz * (ndc.x * upTanHalf.w * forwardAspect.w)
        - upTanHalf.xyz * (ndc.y * upTanHalf.w)
    );
    float3 env = environmentBackgroundTexture.SampleLevel(materialSampler, dir, 0.0).rgb;
    return float4(toneMap(env), 1.0);
}
