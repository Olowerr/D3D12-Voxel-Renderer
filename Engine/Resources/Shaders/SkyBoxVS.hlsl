#include "GPUShared.hlsli"
#include "CubeVerticies.hlsli"

SkyBoxVSOutput main(uint vertexID : SV_VertexID)
{
    SkyBoxVSOutput output;
    float3 vertexPos = CUBE_VERTICIES[vertexID];
    
    output.localPos = vertexPos.xyz;
    output.svPosition = mul(float4(vertexPos + renderCB.cameraPos, 1.f), renderCB.viewProjMatrix).xyww;
    
	return output;
}
