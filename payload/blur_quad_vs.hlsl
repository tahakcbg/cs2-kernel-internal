struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT main(float2 position : POSITION)
{
    VS_OUTPUT output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = float2((position.x + 1.0f) * 0.5f, 1.0f - (position.y + 1.0f) * 0.5f);
    return output;
}
