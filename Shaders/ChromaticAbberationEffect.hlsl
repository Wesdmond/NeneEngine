#include "GBuffer.hlsl"

float4 PS(PS_IN input) : SV_Target
{
    GBufferData buf = ReadGBuffer(input.TexC);
    
    float4 litColor = AfterLightMap.Sample(Sampler, input.TexC);
    
    float2 center = float2(0.5, 0.5);
    float2 offset = input.TexC - center;
    float dist = length(offset);
    float intensity = 0.5;
    
    // Red outsidde, blue inside, green - no offset
    float2 redOffset = offset * dist * intensity;
    float2 blueOffset = -offset * dist * intensity;
    
    float red = AfterLightMap.Sample(Sampler, input.TexC + redOffset).r;
    float green = AfterLightMap.Sample(Sampler, input.TexC).g; // Green without offset
    float blue = AfterLightMap.Sample(Sampler, input.TexC + blueOffset).b;
    float alpha = AfterLightMap.Sample(Sampler, input.TexC).a;
    
    return float4(red, green, blue, alpha);

    return litColor;
}