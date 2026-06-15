struct VertexOut {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

VertexOut main(uint vertexId : SV_VertexID) {
    float2 position = float2((vertexId == 2) ? 3.0 : -1.0, (vertexId == 1) ? 3.0 : -1.0);
    VertexOut output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = position * float2(0.5, 0.5) + 0.5;
    output.uv.y = 1.0 - output.uv.y;
    return output;
}
