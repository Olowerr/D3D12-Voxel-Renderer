
#include "GPUShared.hlsli"
#include "SampleVectors.hlsli"

static const float RADIUS = 0.5f;

float ssaoSample(float3 worldPos, uint offsetIdx, float3 normal, float3x3 tbnMatrix)
{
    Texture2D worldPosGBuffer = gBuffers[1];
    
    uint2 dims;
    worldPosGBuffer.GetDimensions(dims.x, dims.y);
    float2 texelSize = 1.f / (float2)dims;
    
    //float3 offset = offsets[offsetIdx];
    float3 offset = mul(offsets[offsetIdx], tbnMatrix);
    if (dot(offset, normal) < 0.15f)
        return -1.f;
    
    worldPos += offset * RADIUS;
    
    float4 ndc = mul(float4(worldPos, 1.f), renderCB.viewProjMatrix);
    ndc.xyz /= ndc.w;
    ndc.xy = float2(ndc.x * 0.5f + 0.5f, ndc.y * -0.5f + 0.5);
    if (ndc.x < 0.f || ndc.x > 1.f || ndc.y < 0.f || ndc.y > 1.f)
        return 1.f;
    
    float3 samplePos = worldPosGBuffer.SampleLevel(pointSampler, ndc.xy, 0.f).xyz;

    float sampleDepth = length(renderCB.cameraPos - samplePos);
    float pointDepth = length(renderCB.cameraPos - worldPos);
    
    if (abs(pointDepth - sampleDepth) > RADIUS)
        return 1.f;
    
    return sampleDepth > pointDepth - 0.2f ? 1.f : 0.f;
}

float getSSAOValue(inout uint seed, float3 worldPos, float3 normal)
{
    float3 randomVec = randomVectors[pcgHash(seed) % 16];
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 biTangent = cross(tangent, normal);
    
    float3x3 tbnMatrix =
    {
        tangent,
        biTangent,
        normal,
    };
    
    const uint NUM_SSAO_SAMPLES = 64; // 64 max atm
    float ssaoValue = 0.f;
    float numSamples = 0.f;
    for (uint i = 0; i < NUM_SSAO_SAMPLES; i++)
    {
        float result = ssaoSample(worldPos, i, normal, tbnMatrix);
        if (result < -0.5f)
            continue;
        
        ssaoValue += result;
        numSamples += 1.f;
    }
    
    return ssaoValue * (1.f / numSamples);
}

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
    
    uint2 dims;
    diffuseGBuffer.GetDimensions(dims.x, dims.y);

    uint seed = DTid.x + (DTid.y + 74813) * dims.x;

    float ssaoValue = getSSAOValue(seed, worldPos, normal);
    backBuffer[DTid.xy] = float4(ssaoValue, ssaoValue, ssaoValue, 0.f);
    return;

    backBuffer[DTid.xy] = float4(diffuse.rgb * ssaoValue, 0.f);

    float3 SUN_DIR = -normalize(float3(-0.469, -0.820, -0.327));
    float lightIntensity = max(dot(SUN_DIR, normal), 0.3f) * 1.2f;
    
    backBuffer[DTid.xy] = float4(diffuse.rgb * lightIntensity, 0.f);
}
