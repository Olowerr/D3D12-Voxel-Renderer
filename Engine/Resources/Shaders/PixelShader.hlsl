
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    float4 textureColor = textureSheet.Sample(pointSampler, input.uv);
    if (round(textureColor.a) == 0.f)
        discard;
    
    float3 SUN_DIR = -normalize(float3(-0.469, -0.820, -0.327));
   
    float lightIntensity = max(dot(SUN_DIR, SIDE_NORMALS[input.sideIdx]), 0.3f) * 1.2f;
    return float4(textureColor.rgb * lightIntensity, 0.f);
}
