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
    float4 v4Flags;
};

[[vk::binding(5, 0)]] SamplerState materialSampler;
[[vk::binding(15, 0)]] Texture2D<float4> gbufferAlbedo;
[[vk::binding(16, 0)]] Texture2D<float4> gbufferNormal;
[[vk::binding(17, 0)]] Texture2D<float4> gbufferWorld;
[[vk::binding(18, 0)]] Texture2D<float> ssaoRaw;
[[vk::binding(19, 0)]] Texture2D<float> ssaoBlur;

struct ManyLight {
    float4 positionRadius;
    float4 colorIntensity;
};

[[vk::binding(20, 0)]] StructuredBuffer<ManyLight> manyLightBuffer;

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

float3 pointPbr(float3 lightPos, float lightRadius, float3 lightColor, float lightIntensity, float3 worldPos, float3 normal, float3 view, float3 base, float roughness, float metalness, float3 f0) {
    float3 toLight = lightPos - worldPos;
    float distanceToLight = length(toLight);
    float radius = max(0.1, lightRadius);
    if (distanceToLight >= radius) {
        return 0.0.xxx;
    }
    float3 lightDir = toLight / max(distanceToLight, 0.001);
    float ndotl = saturate(dot(normal, lightDir));
    if (ndotl <= 0.0) {
        return 0.0.xxx;
    }

    float falloff = saturate(1.0 - distanceToLight / radius);
    falloff = falloff * falloff;
    float3 halfVector = normalize(lightDir + view);
    float ndotv = max(0.04, saturate(dot(normal, view)));
    float ndoth = saturate(dot(normal, halfVector));
    float alpha = max(0.035, roughness * roughness);
    float alpha2 = alpha * alpha;
    float denom = ndoth * ndoth * (alpha2 - 1.0) + 1.0;
    float distribution = alpha2 / max(0.001, 3.14159265 * denom * denom);
    float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
    float geometry = ndotl / max(0.001, ndotl * (1.0 - k) + k);
    geometry *= ndotv / max(0.001, ndotv * (1.0 - k) + k);
    float3 fresnel = f0 + (1.0.xxx - f0) * pow(1.0 - ndotv, 5.0);
    float3 specular = distribution * geometry * fresnel / max(0.001, 4.0 * ndotl * ndotv);
    float3 diffuse = base * (1.0 - metalness) * (1.0.xxx - fresnel) * 0.31830988;
    return lightColor * lightIntensity * falloff * ndotl * (diffuse + specular);
}

float3 manyLightsRadiance(float3 worldPos, float3 normal, float3 view, float3 base, float roughness, float metalness, float3 f0) {
    if (v4Flags.x <= 0.5) {
        return 0.0.xxx;
    }

    float3 radiance = 0.0.xxx;
    uint lightCount = min((uint)max(0.0, v4Flags.y), 1024u);
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        ManyLight light = manyLightBuffer[i];
        radiance += pointPbr(
            light.positionRadius.xyz,
            light.positionRadius.w,
            light.colorIntensity.rgb,
            light.colorIntensity.w * v4Flags.w,
            worldPos,
            normal,
            view,
            base,
            roughness,
            metalness,
            f0
        );
    }
    return radiance;
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
    uint debugMode = (uint)(v4Flags.z + 0.5);
    if (debugMode == 1) {
        return float4(base, 1.0);
    }
    if (debugMode == 2) {
        return float4(normal * 0.5 + 0.5, 1.0);
    }
    if (debugMode == 3) {
        float depth = saturate(dot(worldPos - eyeNear.xyz, forwardAspect.xyz) / max(0.001, rightFar.w));
        return float4(depth.xxx, 1.0);
    }
    if (debugMode == 4) {
        return float4(ssaoRaw.SampleLevel(materialSampler, input.uv, 0.0).xxx, 1.0);
    }
    if (debugMode == 5) {
        return float4(ssaoBlur.SampleLevel(materialSampler, input.uv, 0.0).xxx, 1.0);
    }

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
    float3 localLights = manyLightsRadiance(worldPos, normal, view, base, roughness, metalness, f0);
    float contact = pow(1.0 - ao, 2.0);
    float3 debugTint = contact.xxx * float3(0.02, 0.035, 0.055);
    return float4(toneMap(ambient + direct + highlight + localLights * ao - debugTint), 1.0);
}
