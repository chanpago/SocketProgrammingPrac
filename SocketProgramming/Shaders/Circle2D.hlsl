cbuffer CircleConstants : register(b0)
{
    float aspectScaleX;
    float3 padding;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float3 position = input.position;
    position.x *= aspectScaleX;
    output.position = float4(position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return input.color;
}
