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

[[vk::binding(1, 0), vk::image_format("rgba8")]]
RWTexture2D<float4> outputImage : register(u1);

static const float kPi = 3.14159265;

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float3 sky(float3 dir) {
    float t = saturate(dir.z * 0.5 + 0.5);
    float3 horizon = float3(1.04, 0.82, 0.56);
    float3 zenith = float3(0.18, 0.38, 0.78);
    float3 ground = float3(0.16, 0.15, 0.13);
    float sun = pow(saturate(dot(dir, normalize(float3(-0.28, -0.46, 0.84)))), 320.0);
    return lerp(ground, lerp(horizon, zenith, t), t) + sun.xxx * float3(6.0, 4.8, 3.2);
}

bool hitSphere(float3 origin, float3 dir, float3 center, float radius, float maxT, out float t, out float3 normal, out float3 color, out float roughness, out float metalness) {
    t = maxT;
    normal = 0.0.xxx;
    color = 0.0.xxx;
    roughness = 0.5;
    metalness = 0.0;
    float3 oc = origin - center;
    float b = dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) {
        return false;
    }
    h = sqrt(h);
    float candidate = -b - h;
    if (candidate < 0.02) {
        candidate = -b + h;
    }
    if (candidate < 0.02 || candidate >= maxT) {
        return false;
    }
    t = candidate;
    float3 p = origin + dir * t;
    normal = normalize(p - center);
    color = float3(0.74, 0.78, 0.82);
    roughness = 0.24;
    metalness = 0.65;
    return true;
}

bool hitPlane(float3 origin, float3 dir, float maxT, out float t, out float3 normal, out float3 color, out float roughness, out float metalness) {
    t = maxT;
    normal = 0.0.xxx;
    color = 0.0.xxx;
    roughness = 0.5;
    metalness = 0.0;
    if (abs(dir.z) < 0.0001) {
        return false;
    }
    float candidate = -origin.z / dir.z;
    if (candidate < 0.02 || candidate >= maxT) {
        return false;
    }
    t = candidate;
    normal = float3(0.0, 0.0, 1.0);
    float2 cell = floor((origin.xy + dir.xy * t) * 0.75);
    float checker = fmod(cell.x + cell.y, 2.0);
    color = lerp(float3(0.58, 0.59, 0.54), float3(0.36, 0.38, 0.39), checker);
    roughness = 0.72;
    metalness = 0.0;
    return true;
}

bool traceScene(float3 origin, float3 dir, out float t, out float3 normal, out float3 color, out float roughness, out float metalness) {
    t = 1.0e20;
    normal = 0.0.xxx;
    color = 0.0.xxx;
    roughness = 0.5;
    metalness = 0.0;
    bool hit = false;

    float sphereT;
    float3 sphereN;
    float3 sphereC;
    float sphereRoughness;
    float sphereMetalness;
    if (hitSphere(origin, dir, float3(-1.25, 0.10, 0.92), 0.92, t, sphereT, sphereN, sphereC, sphereRoughness, sphereMetalness)) {
        t = sphereT; normal = sphereN; color = sphereC; roughness = sphereRoughness; metalness = sphereMetalness; hit = true;
    }
    if (hitSphere(origin, dir, float3(1.20, 0.18, 0.62), 0.62, t, sphereT, sphereN, sphereC, sphereRoughness, sphereMetalness)) {
        t = sphereT; normal = sphereN; color = float3(0.88, 0.42, 0.28); roughness = 0.38; metalness = 0.08; hit = true;
    }
    if (hitSphere(origin, dir, float3(0.05, 1.55, 0.46), 0.46, t, sphereT, sphereN, sphereC, sphereRoughness, sphereMetalness)) {
        t = sphereT; normal = sphereN; color = float3(0.38, 0.64, 0.92); roughness = 0.18; metalness = 0.35; hit = true;
    }
    if (hitPlane(origin, dir, t, sphereT, sphereN, sphereC, sphereRoughness, sphereMetalness)) {
        t = sphereT; normal = sphereN; color = sphereC; roughness = sphereRoughness; metalness = sphereMetalness; hit = true;
    }
    return hit;
}

float3 shadeHit(float3 origin, float3 dir, float t, float3 normal, float3 base, float roughness, float metalness) {
    float3 pos = origin + dir * t;
    float3 lightDir = normalize(float3(-0.42, -0.55, 0.72));
    float3 view = normalize(-dir);
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float specPower = lerp(128.0, 12.0, roughness);
    float spec = pow(saturate(dot(normal, halfVector)), specPower);
    float3 direct = base * ndotl * float3(1.25, 1.12, 0.95);
    float3 ambient = base * 0.16;
    float3 reflected = sky(reflect(dir, normal)) * lerp(0.08, 0.72, metalness) * (1.0 - roughness * 0.55);
    float shadow = 1.0;
    float shadowT;
    float3 shadowN;
    float3 shadowC;
    float shadowRoughness;
    float shadowMetalness;
    if (traceScene(pos + normal * 0.025, lightDir, shadowT, shadowN, shadowC, shadowRoughness, shadowMetalness) && shadowT < 12.0) {
        shadow = 0.28;
    }
    return ambient + direct * shadow + spec.xxx * 0.35 + reflected;
}

float3 toneMap(float3 color) {
    color = color / (color + 1.0.xxx);
    return pow(saturate(color), 1.0 / 2.2);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width;
    uint height;
    outputImage.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) {
        return;
    }

    float2 pixel = (float2(id.xy) + 0.5) / float2(width, height);
    float2 ndc = pixel * 2.0 - 1.0;

    float3 origin = eyeNear.xyz;
    float3 dir = normalize(forwardAspect.xyz + rightFar.xyz * ndc.x * forwardAspect.w * upTanHalf.w + upTanHalf.xyz * -ndc.y * upTanHalf.w);
    float t;
    float3 normal;
    float3 base;
    float roughness;
    float metalness;
    float3 color = sky(dir);
    if (traceScene(origin, dir, t, normal, base, roughness, metalness)) {
        color = shadeHit(origin, dir, t, normal, base, roughness, metalness);
    }
    outputImage[id.xy] = float4(toneMap(color), 1.0);
}
