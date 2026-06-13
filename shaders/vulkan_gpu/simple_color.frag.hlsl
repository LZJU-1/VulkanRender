struct FragmentIn {
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 worldPos : TEXCOORD0;
    [[vk::location(3)]] float roughness : TEXCOORD1;
    [[vk::location(4)]] float metalness : TEXCOORD2;
    [[vk::location(5)]] float materialKind : TEXCOORD3;
};

cbuffer Camera : register(b0) {
    float4 eyeNear;
    float4 rightFar;
    float4 upTanHalf;
    float4 forwardAspect;
};

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

float4 main(FragmentIn input) : SV_Target0 {
    float3 normal = normalize(input.normal);
    float3 view = normalize(eyeNear.xyz - input.worldPos);
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    float3 lightDir = normalize(float3(-0.35, -0.55, 0.76));
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float ndotv = max(0.04, saturate(dot(normal, view)));
    float roughness = clamp(input.roughness, 0.03, 1.0);
    float metalness = saturate(input.metalness);
    float materialKind = input.materialKind;

    float3 base = saturate(input.color);
    float3 reflected = reflect(-view, normal);
    float3 env = skyRadiance(reflected);
    float3 ambient = skyRadiance(normal) * 0.20;

    float specPower = max(2.0, (1.0 - roughness) * 112.0 + 4.0);
    float specTerm = pow(saturate(dot(normal, halfVector)), specPower) * (1.0 - roughness * 0.72);
    float3 f0 = lerp(0.04.xxx, base, metalness);
    float3 fresnel = fresnelSchlick(ndotv, f0);
    float rim = pow(1.0 - ndotv, 3.0) * 0.15;

    float3 hdr;
    if (materialKind > 1.5 && materialKind < 2.5) {
        hdr = env * 1.25;
    } else if (materialKind > 3.5) {
        float3 diffuse = base * (1.0 - metalness) * (ambient + float3(1.15, 1.05, 0.92) * ndotl);
        float3 specular = fresnel * env * (0.25 + specTerm * 1.25);
        hdr = diffuse + specular + rim.xxx * float3(0.35, 0.45, 0.70);
    } else if (materialKind > 2.5) {
        hdr = base * (ambient + float3(1.20, 1.08, 0.92) * ndotl);
    } else if (materialKind > 0.5) {
        hdr = env;
    } else {
        hdr = base * (0.24 + 0.80 * ndotl) + specTerm.xxx * 0.18;
    }

    return float4(toneMap(hdr), 1.0);
}
