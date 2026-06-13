struct FragmentIn {
    [[vk::location(0)]] float3 color : COLOR0;
};

float4 main(FragmentIn input) : SV_Target0 {
    return float4(pow(saturate(input.color), 1.0 / 2.2), 1.0);
}

