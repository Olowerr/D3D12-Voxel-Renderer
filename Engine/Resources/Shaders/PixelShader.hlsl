
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    float3 textureColor = textureSheet.Sample(pointSampler, input.uv).rgb;
    return float4(textureColor, 0.f);
}
