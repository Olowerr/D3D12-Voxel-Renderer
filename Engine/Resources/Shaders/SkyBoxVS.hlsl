#include "GPUShared.hlsli"

static const float4 CUBE[] =
{
    float4(-0.5, 0.5, -0.5, 1),
    float4(-0.5, 0.5, 0.5, 1),
    float4( 0.5, 0.5, 0.5, 1),

    float4(0.5, 0.5, 0.5, 1),
    float4(0.5, 0.5, -0.5, 1),
    float4(-0.5, 0.5, -0.5, 1),


    float4(0.5, -0.5, 0.5, 1),
    float4(-0.5, -0.5, 0.5, 1),
    float4(-0.5, -0.5, -0.5, 1),

    float4(-0.5, -0.5, -0.5, 1),
    float4(0.5, -0.5, -0.5, 1),
    float4(0.5, -0.5, 0.5, 1),


    float4(0.5, 0.5, -0.5, 1),
    float4(0.5, 0.5, 0.5, 1),
    float4(0.5, -0.5, 0.5, 1),

    float4(0.5, 0.5, -0.5, 1),
    float4(0.5, -0.5, 0.5, 1),
    float4(0.5, -0.5, -0.5, 1),


    float4(-0.5, -0.5, 0.5, 1),
    float4(-0.5, 0.5, 0.5, 1),
    float4(-0.5, 0.5, -0.5, 1),

    float4(-0.5, -0.5, -0.5, 1),
    float4(-0.5, -0.5, 0.5, 1),
    float4(-0.5, 0.5, -0.5, 1),


    float4(0.5, 0.5, 0.5, 1),
    float4(-0.5, 0.5, 0.5, 1),
    float4(-0.5, -0.5, 0.5, 1),

    float4(-0.5, -0.5, 0.5, 1),
    float4(0.5, -0.5, 0.5, 1),
    float4(0.5, 0.5, 0.5, 1),


    float4(-0.5, -0.5, -0.5, 1),
    float4(-0.5, 0.5, -0.5, 1),
    float4(0.5, 0.5, -0.5, 1),

    float4(0.5, 0.5, -0.5, 1),
    float4(0.5, -0.5, -0.5, 1),
    float4(-0.5, -0.5, -0.5, 1),

};

SkyBoxVSOutput main(uint vertexID : SV_VertexID)
{
    SkyBoxVSOutput output;
    float4 vertexPos = CUBE[vertexID];
    
    output.localPos = vertexPos.xyz;
    output.svPosition = mul(vertexPos + float4(renderCB.cameraPos, 0.f), renderCB.viewProjMatrix).xyww;
    
	return output;
}
