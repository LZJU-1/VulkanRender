[[vk::binding(1, 0), vk::image_format("rgba8")]]
RWTexture2D<float4> outputImage : register(u1);

[[vk::binding(32, 0), vk::image_format("rgba16f")]]
RWTexture2D<float4> resolvedColor : register(u32);

float luma(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint outWidth;
    uint outHeight;
    outputImage.GetDimensions(outWidth, outHeight);
    if (id.x >= outWidth || id.y >= outHeight) {
        return;
    }

    uint inWidth;
    uint inHeight;
    resolvedColor.GetDimensions(inWidth, inHeight);
    uint2 maxPixel = uint2(max(inWidth, 1u) - 1u, max(inHeight, 1u) - 1u);

    float2 scale = float2(inWidth, inHeight) / float2(max(outWidth, 1u), max(outHeight, 1u));
    uint2 basePixel = min(uint2(floor(float2(id.xy) * scale)), maxPixel);
    if (scale.x < 1.5 && scale.y < 1.5) {
        outputImage[id.xy] = float4(saturate(resolvedColor[basePixel].rgb), 1.0);
        return;
    }

    uint2 stepPixel = uint2(max(1u, (uint)round(scale.x)), max(1u, (uint)round(scale.y)));
    float3 c00 = resolvedColor[min(basePixel + uint2(0u, 0u), maxPixel)].rgb;
    float3 c10 = resolvedColor[min(basePixel + uint2(stepPixel.x - 1u, 0u), maxPixel)].rgb;
    float3 c01 = resolvedColor[min(basePixel + uint2(0u, stepPixel.y - 1u), maxPixel)].rgb;
    float3 c11 = resolvedColor[min(basePixel + stepPixel - uint2(1u, 1u), maxPixel)].rgb;
    float3 color = (c00 + c10 + c01 + c11) * 0.25;

    float contrast = max(max(luma(c00), luma(c10)), max(luma(c01), luma(c11))) - min(min(luma(c00), luma(c10)), min(luma(c01), luma(c11)));
    float3 diagonalA = (c00 + c11) * 0.5;
    float3 diagonalB = (c10 + c01) * 0.5;
    float diagonalWeight = saturate(contrast * 0.85);
    color = lerp(color, (diagonalA + diagonalB) * 0.5, diagonalWeight * 0.12);

    outputImage[id.xy] = float4(saturate(color), 1.0);
}
