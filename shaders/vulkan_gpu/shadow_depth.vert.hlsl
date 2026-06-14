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

float4 main(VertexIn input) : SV_Position {
    float3 offset = input.position - shadowCenterBias.xyz;
    float extent = max(0.01, shadowRightExtent.w);
    float nearPlane = shadowUpNear.w;
    float farPlane = shadowForwardFar.w;
    float x = dot(offset, shadowRightExtent.xyz) / extent;
    float y = -dot(offset, shadowUpNear.xyz) / extent;
    float z = (dot(offset, shadowForwardFar.xyz) - nearPlane) / max(0.01, farPlane - nearPlane);
    return float4(x, y, z, 1.0);
}
