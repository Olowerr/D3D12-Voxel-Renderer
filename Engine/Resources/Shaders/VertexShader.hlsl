
static const float3 tris[3] =
{
	float3(-0.5f, -0.5f, 0.f),
	float3(0.f, 0.5f, 0.f),
	float3(0.5f, -0.5f, 0.f),
};

static const float4 colors[3] =
{
    float4(1.f, 0.f, 0.f, 0.f),
    float4(0.f, 1.f, 0.f, 0.f),
    float4(0.f, 0.f, 1.f, 0.f),
};

struct VSOutput
{
    float4 svPosition : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    output.svPosition = float4(tris[vertexId], 1.f);
    output.color = colors[vertexId];
    
    return output;
}
