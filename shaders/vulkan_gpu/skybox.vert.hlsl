struct VertexOut {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 ndc : TEXCOORD0;
};

VertexOut main(uint vertexId : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    VertexOut output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.ndc = positions[vertexId];
    return output;
}
