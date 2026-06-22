struct VertexIn {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 color : COLOR0;
    [[vk::location(3)]] float2 uv : TEXCOORD0;
    [[vk::location(4)]] float textured : TEXCOORD1;
    [[vk::location(5)]] float roughness : TEXCOORD2;
    [[vk::location(6)]] float metalness : TEXCOORD3;
    [[vk::location(7)]] float materialKind : TEXCOORD4;
    [[vk::location(8)]] float4 tangent : TANGENT;
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

float3 normalizedOr(float3 v, float3 fallback) {
    float len2 = dot(v, v);
    return len2 > 1e-6 ? v * rsqrt(len2) : fallback;
}

void pointFaceBasis(uint face, out float3 forward, out float3 right, out float3 up) {
    if (face == 0) {
        forward = float3(1.0, 0.0, 0.0);
        right = float3(0.0, 1.0, 0.0);
        up = float3(0.0, 0.0, 1.0);
    } else if (face == 1) {
        forward = float3(-1.0, 0.0, 0.0);
        right = float3(0.0, -1.0, 0.0);
        up = float3(0.0, 0.0, 1.0);
    } else if (face == 2) {
        forward = float3(0.0, 1.0, 0.0);
        right = float3(-1.0, 0.0, 0.0);
        up = float3(0.0, 0.0, 1.0);
    } else if (face == 3) {
        forward = float3(0.0, -1.0, 0.0);
        right = float3(1.0, 0.0, 0.0);
        up = float3(0.0, 0.0, 1.0);
    } else if (face == 4) {
        forward = float3(0.0, 0.0, 1.0);
        right = float3(1.0, 0.0, 0.0);
        up = float3(0.0, 1.0, 0.0);
    } else {
        forward = float3(0.0, 0.0, -1.0);
        right = float3(1.0, 0.0, 0.0);
        up = float3(0.0, -1.0, 0.0);
    }
}

float4 directionalCascadePosition(float3 worldPos, uint cascadeIndex) {
    float extent = cascadeIndex == 0 ? v3Flags.y : (cascadeIndex == 1 ? v3Flags.z : v3Flags.w);
    extent = max(0.01, extent);
    float nearPlane = shadowUpNear.w;
    float farPlane = shadowForwardFar.w;
    float3 offset = worldPos - shadowCenterBias.xyz;
    float x = dot(offset, shadowRightExtent.xyz) / extent;
    float y = -dot(offset, shadowUpNear.xyz) / extent;
    float z = (dot(offset, shadowForwardFar.xyz) - nearPlane) / max(0.01, farPlane - nearPlane);
    return float4(x, y, z, 1.0);
}

float4 spotShadowPosition(float3 worldPos) {
    float3 forward = normalizedOr(spotDirOuter.xyz, float3(0.0, 0.0, -1.0));
    float3 right = normalizedOr(cross(float3(0.0, 0.0, 1.0), forward), float3(1.0, 0.0, 0.0));
    float3 up = normalizedOr(cross(forward, right), float3(0.0, 1.0, 0.0));
    float3 toPoint = worldPos - spotPosInner.xyz;
    float forwardDistance = dot(toPoint, forward);
    float mode = floor(v3Flags.x + 0.5);
    float coneTan = tan(lerp(34.0, 36.0, 1.0 - saturate(abs(mode - 5.0))) * 3.14159265 / 180.0);
    float x = dot(toPoint, right) / max(0.05, forwardDistance * coneTan);
    float y = -dot(toPoint, up) / max(0.05, forwardDistance * coneTan);
    float spotFar = lerp(6.5, 7.4, 1.0 - saturate(abs(mode - 5.0)));
    float z = (forwardDistance - 0.05) / spotFar;
    return float4(x, y, z, 1.0);
}

float4 pointShadowPosition(float3 worldPos, uint face) {
    float3 forward;
    float3 right;
    float3 up;
    pointFaceBasis(face, forward, right, up);
    float3 toPoint = worldPos - pointPosRadius.xyz;
    float forwardDistance = dot(toPoint, forward);
    float x = dot(toPoint, right) / max(0.05, forwardDistance);
    float y = -dot(toPoint, up) / max(0.05, forwardDistance);
    float z = length(toPoint) / max(0.05, pointPosRadius.w);
    return float4(x, y, z, 1.0);
}

float4 main(VertexIn input, uint instanceId : SV_InstanceID, uint startInstance : SV_StartInstanceLocation) : SV_Position {
    const uint shadowIndex = startInstance + instanceId;
    if (shadowIndex < 3) {
        return directionalCascadePosition(input.position, shadowIndex);
    }
    if (shadowIndex == 3) {
        return spotShadowPosition(input.position);
    }
    return pointShadowPosition(input.position, shadowIndex - 4);
}
