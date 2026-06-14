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
[[vk::binding(6, 0)]] Texture2D<float4> environmentBackgroundTexture;

float3 toneMap(float3 hdr) {
    float3 mapped = hdr / (hdr + 1.0.xxx);
    return pow(saturate(mapped), 1.0 / 2.2);
}

float3 unpackRgbe(float4 rgbe) {
    float exponent = rgbe.a * 255.0 - 128.0;
    return exp2(exponent) * (rgbe.rgb * 255.0 + 0.5) / 256.0;
}

float2 cubeStripUv(float3 dir) {
    dir = normalize(dir);
    float3 absDir = abs(dir);
    float face = 0.0;
    float2 uv = 0.5.xx;

    if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
        float ma = absDir.x;
        if (dir.x >= 0.0) {
            face = 0.0;
            uv = float2(-dir.z, -dir.y) / ma;
        } else {
            face = 1.0;
            uv = float2(dir.z, -dir.y) / ma;
        }
    } else if (absDir.y >= absDir.z) {
        float ma = absDir.y;
        if (dir.y >= 0.0) {
            face = 2.0;
            uv = float2(dir.x, dir.z) / ma;
        } else {
            face = 3.0;
            uv = float2(dir.x, -dir.z) / ma;
        }
    } else {
        float ma = absDir.z;
        if (dir.z >= 0.0) {
            face = 4.0;
            uv = float2(dir.x, -dir.y) / ma;
        } else {
            face = 5.0;
            uv = float2(-dir.x, -dir.y) / ma;
        }
    }

    uv = uv * 0.5 + 0.5;
    return float2(uv.x, (face + uv.y) / 6.0);
}

float3 sampleEnvironmentCubeStrip(Texture2D<float4> textureMap, float3 dir) {
    return unpackRgbe(textureMap.SampleLevel(materialSampler, cubeStripUv(dir), 0.0));
}

float4 main(FragmentIn input) : SV_Target0 {
    float2 ndc = input.ndc;
    float3 dir = normalize(
        forwardAspect.xyz
        + rightFar.xyz * (ndc.x * upTanHalf.w * forwardAspect.w)
        - upTanHalf.xyz * (ndc.y * upTanHalf.w)
    );
    float3 env = sampleEnvironmentCubeStrip(environmentBackgroundTexture, dir);
    return float4(toneMap(env), 1.0);
}
