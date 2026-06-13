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
[[vk::binding(7, 0)]] Texture2D<float4> environmentSpecularR0Texture;

static const float PI = 3.14159265;

float3 toneMap(float3 hdr) {
    float3 mapped = hdr / (hdr + 1.0.xxx);
    return pow(saturate(mapped), 1.0 / 2.2);
}

float3 skyRadiance(float3 dir) {
    dir = normalize(dir);
    float horizon = saturate(dir.z * 0.5 + 0.5);
    float3 ground = float3(0.18, 0.16, 0.13);
    float3 lowSky = float3(1.18, 0.80, 0.46);
    float3 highSky = float3(0.20, 0.43, 0.90);
    float3 sky = lerp(ground, lerp(lowSky, highSky, horizon), horizon);
    float sun = pow(saturate(dot(dir, normalize(float3(-0.30, -0.62, 0.72)))), 280.0);
    return sky * 1.2 + sun.xxx * float3(9.0, 7.0, 4.2);
}

float2 environmentUv(float3 dir) {
    dir = normalize(dir);
    float u = atan2(dir.y, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - acos(clamp(dir.z, -1.0, 1.0)) / PI;
    return float2(u, v);
}

float4 main(FragmentIn input) : SV_Target0 {
    float2 ndc = input.ndc;
    float3 dir = normalize(
        forwardAspect.xyz
        + rightFar.xyz * (ndc.x * upTanHalf.w * forwardAspect.w)
        - upTanHalf.xyz * (ndc.y * upTanHalf.w)
    );
    float3 env = environmentSpecularR0Texture.Sample(materialSampler, environmentUv(dir)).rgb * 2.2;
    return float4(toneMap(lerp(skyRadiance(dir), env, 0.92)), 1.0);
}
