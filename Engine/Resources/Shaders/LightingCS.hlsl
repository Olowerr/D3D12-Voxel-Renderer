
#include "GPUShared.hlsli"

[numthreads(16, 9, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const float GAMMA = 2.2f;
    const float INV_GAMMA = 1.f / GAMMA;
    
    // temp lol
    float3 SUN_DIR = -normalize(float3(-0.469, -0.820, -0.327));

    Texture2D diffuseGBuffer = gBuffers[0];
    Texture2D worldPosGBuffer = gBuffers[1];
    Texture2D normalGBuffer = gBuffers[2];
    
    float3 diffuse = pow(diffuseGBuffer[DTid.xy].xyz, float3(GAMMA, GAMMA, GAMMA));
    float3 ambient = diffuse * 0.2f;
    float3 worldPos = worldPosGBuffer[DTid.xy].xyz;
    float3 normal = normalGBuffer[DTid.xy].xyz;
    float ambientOcclusion = ssaoBuffer[DTid.xy].r;
    
    float lightIntensity = max(dot(SUN_DIR, normal), 0.f);

    float3 finalColour = (ambient + diffuse * lightIntensity) * ambientOcclusion;
    
    finalColour = pow(finalColour, float3(INV_GAMMA, INV_GAMMA, INV_GAMMA));
    mainBuffer[DTid.xy] = float4(finalColour, 0.f);
}
