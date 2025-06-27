
#include "GPUShared.hlsli"

VoxelVSOutput main(uint vertexId : SV_VertexID)
{
    VoxelVSOutput output;

    Vertex vertex = verticies[vertexId];
    
    output.svPosition = mul(float4(vertex.position, 1.f), renderCB.viewProjMatrix);
    output.color = vertex.colour;
    
    return output;
}
