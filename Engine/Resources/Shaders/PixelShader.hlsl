
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    return float4(input.color, 0.f);
}
