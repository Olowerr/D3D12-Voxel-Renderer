
struct RenderData
{
    float4x4 viewProjMatrix;
};

ConstantBuffer<RenderData> renderCB : register(b0, space0);

struct VoxelVSOutput
{
    float4 svPosition : SV_POSITION;
    float4 color : COLOR;
};
