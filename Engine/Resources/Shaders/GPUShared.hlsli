
struct RenderData
{
    float4x4 viewProjMatrix;
    uint2 textureSheetNumTextures;
};

struct VoxelVSOutput
{
    float4 svPosition : SV_POSITION;
    float2 uv : UV;
    uint textureIdx : TEXTURE_IDX;
};

struct Vertex
{
    float3 position;
    float2 uv;
    uint textureIdx;
};

ConstantBuffer<RenderData> renderCB : register(b0, space0);
StructuredBuffer<Vertex> verticies : register(t0, space0);
Texture2D<float4> textureSheet : register(t1, space0);
SamplerState pointSampler : register(s0, space0);