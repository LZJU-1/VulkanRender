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
    [[vk::location(9)]] float4 instancePositionRadius : TEXCOORD7;
    [[vk::location(10)]] float4 instanceColorRoughness : TEXCOORD8;
    [[vk::location(11)]] float2 instanceMetalKind : TEXCOORD9;
};

struct VertexOut {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 worldPos : TEXCOORD0;
    [[vk::location(3)]] float2 uv : TEXCOORD1;
    [[vk::location(4)]] float textured : TEXCOORD2;
    [[vk::location(5)]] float roughness : TEXCOORD3;
    [[vk::location(6)]] float metalness : TEXCOORD4;
    [[vk::location(7)]] float materialKind : TEXCOORD5;
    [[vk::location(8)]] float4 tangent : TEXCOORD6;
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

VertexOut main(VertexIn input) {
    float radius = input.instancePositionRadius.w;
    float3 worldPos = input.instancePositionRadius.xyz + input.position * radius;
    float3 offset = worldPos - eyeNear.xyz;
    float cameraX = dot(offset, rightFar.xyz);
    float cameraY = dot(offset, upTanHalf.xyz);
    float cameraZ = max(dot(offset, forwardAspect.xyz), eyeNear.w);

    float invTan = 1.0 / upTanHalf.w;
    float farPlane = rightFar.w;
    float nearPlane = eyeNear.w;
    float aspect = forwardAspect.w;

    VertexOut output;
    output.position.x = cameraX * invTan / aspect;
    output.position.y = -cameraY * invTan;
    output.position.z = cameraZ * farPlane / (farPlane - nearPlane) - nearPlane * farPlane / (farPlane - nearPlane);
    output.position.w = cameraZ;
    output.color = input.instanceColorRoughness.rgb;
    output.normal = normalize(input.normal);
    output.worldPos = worldPos;
    output.uv = input.uv;
    output.textured = 0.0;
    output.roughness = input.instanceColorRoughness.w;
    output.metalness = input.instanceMetalKind.x;
    output.materialKind = input.instanceMetalKind.y;
    output.tangent = input.tangent;
    return output;
}
