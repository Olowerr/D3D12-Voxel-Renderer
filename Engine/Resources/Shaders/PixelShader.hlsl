
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    uv /= renderCB.textureSheetNumTextures;
    uv.x += (1.f / renderCB.textureSheetNumTextures.x) * (input.textureIdx % renderCB.textureSheetNumTextures.x);
    uv.y += (1.f / renderCB.textureSheetNumTextures.y) * (input.textureIdx % renderCB.textureSheetNumTextures.y);
    
    
    float3 textureColor = textureSheet.Sample(pointSampler, float2(uv.x, uv.y)).rgb;

    return float4(textureColor, 0.f);
}
