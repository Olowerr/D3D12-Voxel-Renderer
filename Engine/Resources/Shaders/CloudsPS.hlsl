
struct CloudVSOutput
{
    float4 svPosition : SV_POSITION;
    float4 colour : CLOUD_COLOUR;
};

float4 main(CloudVSOutput input) : SV_TARGET
{
    return input.colour;
}
