
#include "GPUShared.hlsli"

float4 main(VoxelVSOutput input) : SV_TARGET
{
    return input.color;
}
