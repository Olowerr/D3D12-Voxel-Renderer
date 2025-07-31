
static const float3 SIDE_NORMALS[6] =
{
    float3( 0.f,  1.f,  0.f),
    float3( 0.f, -1.f,  0.f),
    float3( 1.f,  0.f,  0.f),
    float3(-1.f,  0.f,  0.f),
    float3( 0.f,  0.f,  1.f),
    float3( 0.f,  0.f, -1.f),
};

struct RenderData
{
    float4x4 viewProjMatrix;
    uint textureSheetTileSize;
    uint textureSheetPadding;
    float2 padding0;
};

struct DrawCallData
{
    float3 chunkWorldPos;
};

struct VoxelVSOutput
{
    float4 svPosition : SV_POSITION;
    float2 uv : UV;
    uint sideIdx : SIDE_IDX;
};

struct Vertex
{
    uint data;
    /*
    vertexData bit layout:
    [(bit type representation) data - bit position | numBits | total bits used]

    (uint) position.x : pos: 0   |  num: 5  |  total: 5
    (uint) position.y : pos: 5   |  num: 9  |  total: 14
    (uint) position.z : pos: 14  |  num: 5  |  total: 19

    (bool) globalUV.x : pos: 19  |  num: 1  |  total: 20
    (bool) globalUV.y : pos: 20  |  num: 1  |  total: 21
    (uint) textureID  : pos: 21  |  num: 8  |  total: 29
    (uint) sideIdx    : pos: 29  |  num: 3  |  total: 32
    */
};

ConstantBuffer<RenderData> renderCB : register(b0, space0);
ConstantBuffer<DrawCallData> drawCB : register(b1, space0);
StructuredBuffer<Vertex> verticies : register(t0, space0);
Texture2D<float4> textureSheet : register(t1, space0);
SamplerState pointSampler : register(s0, space0);

float2 calculateUVCoords(float2 globalUV, uint textureID)
{
    uint2 textureSheetDims;
    textureSheet.GetDimensions(textureSheetDims.x, textureSheetDims.y);
    
    // This calculation breaks if sheetDims.x == TILE_SIZE but it's okay :3
    uint numXTextures = textureSheetDims.x / (renderCB.textureSheetTileSize + renderCB.textureSheetPadding / 2);

    float2 tileCoords = float2(textureID % numXTextures, textureID / numXTextures);
    float2 invSheetDims = 1.f / textureSheetDims;

    float2 uv = globalUV;
    uv *= invSheetDims * (float)renderCB.textureSheetTileSize;
    uv += tileCoords * float(renderCB.textureSheetTileSize + renderCB.textureSheetPadding) * invSheetDims;
    
    return uv;
}


uint extractData(uint data, uint bitPos, uint numBits)
{
    return (data << bitPos) >> (32 - numBits);
}
