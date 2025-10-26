
#include "GPUShared.hlsli"

[numthreads(16, 9, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 dims;
    mainBuffer.GetDimensions(dims.x, dims.y);
    
    int sampleWidth = 10;
    
    float avgValue = 0.f;
    float numSamples = 1.f;
    for (int y = -sampleWidth; y <= sampleWidth; y++)
    {
        for (int x = -sampleWidth; x <= sampleWidth; x++)
        {
            int2 sampleId = DTid.xy + int2(x, y);
            if (sampleId.x < 0 || sampleId.x > (int)dims.x || sampleId.y < 0 || sampleId.y > (int)dims.y)
                continue;

            avgValue += mainBuffer[sampleId].r;
            numSamples += 1.f;
        }
    }
 
    avgValue /= numSamples;
    ssaoBuffer[DTid.xy] = avgValue;
    //ssaoBuffer[DTid.xy] = mainBuffer[DTid.xy];
}
