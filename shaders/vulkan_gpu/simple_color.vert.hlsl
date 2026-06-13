struct VertexIn {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 color : COLOR0;
};

struct VertexOut {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 worldPos : TEXCOORD0;
};

cbuffer Camera : register(b0) {
    float4 eyeNear;
    float4 rightFar;
    float4 upTanHalf;
    float4 forwardAspect;
};

VertexOut main(VertexIn input) {
    float3 offset = input.position - eyeNear.xyz;
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
    output.color = input.color;
    output.normal = normalize(input.normal);
    output.worldPos = input.position;
    return output;
}
