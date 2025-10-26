
#include "GPUShared.hlsli"

static const float RADIUS = 0.5f;
static const float BIAS = 0.1f;

float ssaoSample(float3 worldPos, uint offsetIdx, float3 normal, float3x3 tbnMatrix)
{
    Texture2D worldPosGBuffer = gBuffers[1];
    
    uint2 dims;
    worldPosGBuffer.GetDimensions(dims.x, dims.y);
    float2 texelSize = 1.f / (float2)dims;
    
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

    float sampleDepth = length(samplePos - renderCB.cameraPos);
    float pointDepth = length(worldPos - renderCB.cameraPos);
    
    float3 sampleToWorld = worldPos - samplePos;
    if (dot(sampleToWorld, sampleToWorld) > RADIUS * RADIUS)
        return 1.f;
    
    return sampleDepth > pointDepth - BIAS ? 1.f : 0.f;
}

float getSSAOValue(float3 worldPos, float3 normal, inout uint seed)
{
    const uint NUM_RANDOM_VECTORS = 16;
    float3 randomVec = randomVectors[pcgHash(seed) % NUM_RANDOM_VECTORS];
    float3 tangent = cross(normal, randomVec);
    float3 biTangent = cross(normal, tangent);
    
    float3x3 tbnMatrix =
    {
        tangent,
        biTangent,
        normal,
    };

    const uint NUM_SSAO_SAMPLES = 128; // 64 max atm
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
    mainBuffer.GetDimensions(bbDims.x, bbDims.y);
    if (DTid.x >= bbDims.x || DTid.y >= bbDims.y)
        return;

    Texture2D worldPosGBuffer = gBuffers[1];
    Texture2D normalGBuffer = gBuffers[2];
    
    float3 worldPos = worldPosGBuffer[DTid.xy].xyz;
    float3 normal = normalGBuffer[DTid.xy].xyz;
    if (normal.x == 0.f && normal.y == 0.f && normal.z == 0.f)
    {
        mainBuffer[DTid.xy] = float4(1.f, 1.f, 1.f, 0.f);
        return;
    }
    
    uint2 dims;
    worldPosGBuffer.GetDimensions(dims.x, dims.y);
    
    uint seed = DTid.x + (DTid.y + 543978) * dims.x;

    float ssaoValue = getSSAOValue(worldPos, normal, seed);
    ssaoValue = pow(ssaoValue, 3.f);

    mainBuffer[DTid.xy] = float4(ssaoValue, ssaoValue, ssaoValue, 0.f);
}
