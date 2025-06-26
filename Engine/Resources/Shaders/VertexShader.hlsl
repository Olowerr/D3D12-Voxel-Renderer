
#include "GPUShared.hlsli"

static const float3 tris[3] =
{
	float3(-0.5f, -0.5f, 0.f),
	float3(0.f, 0.5f, 0.f),
	float3(0.5f, -0.5f, 0.f),
};

static const float4 colors[3] =
{
    float4(1.f, 0.f, 0.f, 0.f),
    float4(0.f, 1.f, 0.f, 0.f),
    float4(0.f, 0.f, 1.f, 0.f),
};

VoxelVSOutput main(uint vertexId : SV_VertexID)
{
    VoxelVSOutput output;

    output.svPosition = mul(float4(tris[vertexId], 1.f), renderCB.viewProjMatrix);
    output.color = colors[vertexId];
    
    return output;
}
