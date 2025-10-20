
#include "GPUShared.hlsli"

struct RenderTargets
{
    float4 diffuse : SV_TARGET0;
    float4 worldPos : SV_TARGET1;
    float4 normal : SV_TARGET2;
};

RenderTargets main(VoxelVSOutput input)
{
    float4 textureColor = textureSheet.Sample(pointSampler, input.uv);
    if (round(textureColor.a) == 0.f)
        discard;
    
    RenderTargets output;
    output.diffuse = textureColor;
    output.worldPos = float4(input.worldPos, 0.f);
    output.normal = float4(SIDE_NORMALS[input.sideIdx], 0.f);
    
    return output;
}
