
#include "GPUShared.hlsli"

VoxelVSOutput main(uint vertexId : SV_VertexID)
{
    VoxelVSOutput output;
    Vertex vertex = verticies[vertexId];
    
    // A lot of magic numbers, should maybe change into constants?
    
    float3 position;
    position.x = (float)extractData(vertex.data, 0, 5);
    position.y = (float)extractData(vertex.data, 5, 9);
    position.z = (float)extractData(vertex.data, 14, 5);
    position += drawCB.chunkWorldPos;
    
    float2 globalUV;
    globalUV.x = (float)extractData(vertex.data, 19, 1);
    globalUV.y = (float)extractData(vertex.data, 20, 1);
    
    uint textureID = extractData(vertex.data, 21, 8);
    uint sideIdx = extractData(vertex.data, 29, 3);
    
    output.svPosition = mul(float4(float3(position), 1.f), renderCB.viewProjMatrix);
    output.uv = calculateUVCoords(globalUV, textureID);
    output.sideIdx = sideIdx;
    
    return output;
}
