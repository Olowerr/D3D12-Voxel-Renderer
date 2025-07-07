
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    float3 textureColor = textureSheet.Sample(pointSampler, (input.svPosition.xy / float2(1600.f, 900.f))).rgb;

    return float4(textureColor, 0.f);
    return float4(textureColor * input.color, 0.f);
}
