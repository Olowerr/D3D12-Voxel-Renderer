
Texture2D<unorm float4> source : register(t0, space0);
RWTexture2D<unorm float4> target : register(u0, space0);
SamplerState sampy : register(s0, space0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 targetDims;
    target.GetDimensions(targetDims.x, targetDims.y);
    if (DTid.x >= targetDims.x || DTid.y >= targetDims.y)
        return;

    float2 uv = DTid.xy / float2(targetDims.x, targetDims.y);
    float2 texelSize = 1.f / float2(targetDims.x, targetDims.y);
    target[DTid.xy] = source.SampleLevel(sampy, uv + texelSize * 0.5f, 0.f);
}
