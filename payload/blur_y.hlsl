Texture2D tex : register(t0);
SamplerState samp : register(s0);
cbuffer Constants : register(b0) { float pixelSize; }

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    float4 color = 0;
    float total_weight = 0;

    for(float y = -128; y <= 128; y++) {
        float weight = exp(-(y * y) / 128);
        color += tex.Sample(samp, uv + float2(0, y * pixelSize)) * weight;
        total_weight += weight;
    }

    return color / total_weight;
}
