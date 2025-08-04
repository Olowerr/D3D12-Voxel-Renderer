
#include "GPUShared.hlsli"

float4 main(WaterVSOutput input) : SV_TARGET
{
    float3 textureColor = textureSheet.Sample(pointSampler, input.uv).rgb;
    return float4(textureColor, 0.5f);
}
