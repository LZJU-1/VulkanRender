struct FragmentIn {
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

float4 main(FragmentIn input) : SV_Target0 {
    float3 normal = normalize(input.normal);
    float3 view = normalize(eyeNear.xyz - input.worldPos);
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    float3 lightDir = normalize(float3(-0.35, -0.55, 0.76));
    float ndotl = saturate(dot(normal, lightDir));
    float3 halfVector = normalize(lightDir + view);
    float spec = pow(saturate(dot(normal, halfVector)), 48.0) * 0.25;
    float rim = pow(1.0 - saturate(dot(normal, view)), 3.0) * 0.18;

    float3 base = saturate(input.color);
    float3 lit = base * (0.22 + 0.78 * ndotl) + spec.xxx + rim.xxx * float3(0.55, 0.65, 0.85);
    return float4(pow(saturate(lit), 1.0 / 2.2), 1.0);
}
