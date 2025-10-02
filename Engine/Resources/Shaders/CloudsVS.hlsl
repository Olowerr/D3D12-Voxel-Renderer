#include "GPUShared.hlsli"
#include "CubeVerticies.hlsli"

struct CloudVSOutput
{
    float4 svPosition : SV_POSITION;
    float4 colour : CLOUD_COLOUR;
};

StructuredBuffer<float3> cloudList : register(t0, space0);
cbuffer cloudRenderData : register(b1, space0)
{
    float4 colour;
    float3 offset;
    float scale;
};

CloudVSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    CloudVSOutput output;
    
    float3 vertexPos = CUBE_VERTICIES[vertexId];
    vertexPos *= scale;
    vertexPos += cloudList[instanceId] + offset;

    output.svPosition = mul(float4(vertexPos, 1.f), renderCB.viewProjMatrix);
    output.colour = colour;
    
    return output;
}
