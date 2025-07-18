
struct RenderData
{
    float4x4 viewProjMatrix;
};

struct VoxelVSOutput
{
    float4 svPosition : SV_POSITION;
    float2 uv : UV;
};

struct Vertex
{
    int3 position;
    float2 uv;
};

ConstantBuffer<RenderData> renderCB : register(b0, space0);
StructuredBuffer<Vertex> verticies : register(t0, space0);
Texture2D<float4> textureSheet : register(t1, space0);
SamplerState pointSampler : register(s0, space0);