// debug.hlsl

cbuffer cbPerObject : register(b0)
{
}

Texture2D gDebugTex : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

PSInput VSMain(VSInput vin)
{
    PSInput vout;
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexC = vin.TexC;
    return vout;
}

float4 PSMain(PSInput pin) : SV_Target
{
    //return float4(1, 1, 1, 1);
    return gDebugTex.Sample(gSampler, pin.TexC);
}
