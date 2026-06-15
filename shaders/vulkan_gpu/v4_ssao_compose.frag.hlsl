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
[[vk::binding(15, 0)]] Texture2D<float4> gbufferAlbedo;
[[vk::binding(16, 0)]] Texture2D<float4> gbufferNormal;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;
[[vk::binding(19, 0)]] Texture2D<float> ssaoBlur;

float3 toneMap(float3 hdr) {
    float3 mapped = hdr / (hdr + 1.0.xxx);
    return pow(saturate(mapped), 1.0 / 2.2);
}

float3 skyRadiance(float3 dir) {
    dir = normalize(dir);
    float horizon = saturate(dir.z * 0.5 + 0.5);
    float3 ground = float3(0.20, 0.18, 0.15);
    float3 lowSky = float3(1.05, 0.82, 0.55);
    float3 highSky = float3(0.22, 0.42, 0.82);
    return lerp(ground, lerp(lowSky, highSky, horizon), horizon) * 1.05;
}

float4 main(FragmentIn input) : SV_Target0 {
    float4 worldKind = gbufferWorld.SampleLevel(materialSampler, input.uv, 0.0);
    if (worldKind.w <= 0.5) {
        float2 ndc = input.uv * 2.0 - 1.0;
        float3 ray = normalize(forwardAspect.xyz + rightFar.xyz * ndc.x * forwardAspect.w * upTanHalf.w + upTanHalf.xyz * -ndc.y * upTanHalf.w);
        return float4(toneMap(skyRadiance(ray)), 1.0);
    }

    float4 albedoRoughness = gbufferAlbedo.SampleLevel(materialSampler, input.uv, 0.0);
    float4 normalMetalness = gbufferNormal.SampleLevel(materialSampler, input.uv, 0.0);
    float3 base = saturate(albedoRoughness.rgb);
    float roughness = saturate(albedoRoughness.a);
    float metalness = saturate(normalMetalness.a);
    float3 normal = normalize(normalMetalness.rgb * 2.0 - 1.0);
    float3 worldPos = worldKind.xyz;
    float3 view = normalize(eyeNear.xyz - worldPos);
    float3 lightDir = normalize(-shadowForwardFar.xyz);
    float ndotl = saturate(dot(normal, lightDir));
    float ndotv = max(0.04, saturate(dot(normal, view)));
    float3 halfVector = normalize(lightDir + view);
    float specPower = max(4.0, (1.0 - roughness) * 96.0);
    float specular = pow(saturate(dot(normal, halfVector)), specPower) * (1.0 - roughness * 0.65);
    float ao = saturate(ssaoBlur.SampleLevel(materialSampler, input.uv, 0.0));
    float3 f0 = lerp(0.04.xxx, base, metalness);
    float3 fresnel = f0 + (1.0.xxx - f0) * pow(1.0 - ndotv, 5.0);
    float3 ambient = base * lerp(0.15, 0.42, ao) * ao;
    float3 direct = base * (1.0 - metalness) * float3(1.10, 1.04, 0.92) * ndotl;
    float3 highlight = fresnel * specular * 0.75;
    float contact = pow(1.0 - ao, 2.0);
    float3 debugTint = contact.xxx * float3(0.02, 0.035, 0.055);
    return float4(toneMap(ambient + direct + highlight - debugTint), 1.0);
}
