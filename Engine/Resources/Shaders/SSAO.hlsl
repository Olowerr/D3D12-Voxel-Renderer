
#include "GPUShared.hlsli"

[numthreads(16, 9, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 bbDims;
    backBuffer.GetDimensions(bbDims.x, bbDims.y);
    if (DTid.x >= bbDims.x || DTid.y >= bbDims.y)
        return;

    Texture2D diffuseGBuffer = gBuffers[0];
    Texture2D worldPosGBuffer = gBuffers[1];
    Texture2D normalGBuffer = gBuffers[2];
    
    float3 diffuse = diffuseGBuffer[DTid.xy].rgb;
    float3 worldPos = worldPosGBuffer[DTid.xy].xyz;
    float3 normal = normalGBuffer[DTid.xy].xyz;
    
    
    backBuffer[DTid.xy] = float4(worldPos, 1.f);
}
