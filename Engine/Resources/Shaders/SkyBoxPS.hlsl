#include "GPUShared.hlsli"

float3 getEnvironmentLight(float3 direction)
{
    const static float3 SKY_BASE_COLOUR = float3(102.f, 204.f, 255.f) / 255.f;
    const static float3 GROUND_COLOUR = float3(164.f, 177.f, 178.f) / 255.f;
    
    const float dotty = dot(direction, float3(0.f, 1.f, 0.f));
    const float transitionDotty = clamp((dotty + 0.005f) / 0.01f, 0.f, 1.f);
    
    float3 skyColour = lerp(float3(1.f, 1.f, 1.f), SKY_BASE_COLOUR, pow(max(dotty, 0.f), 0.6f));
    return lerp(GROUND_COLOUR, skyColour, transitionDotty);
}

float4 main(SkyBoxVSOutput vsOutput) : SV_TARGET
{
    float3 skyDir = normalize(vsOutput.localPos);
    float3 environmentLight = getEnvironmentLight(skyDir);
    
    return float4(environmentLight, 1.0f);
}
