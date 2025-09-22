#include "GBuffer.hlsl"

float4 PS(PS_IN input) : SV_Target
{
    float2 center = float2(0.5, 0.5);
    float2 offset = input.TexC - center;
    float dist = length(offset);
    float intensity = 0.5;
    
    //float2 redOffset = float2(0.02, 0.0);
    //float2 blueOffset = float2(-0.02, 0.0);
    
    // Red outside, blue inside, green - no offset
    float2 redOffset = offset * dist * intensity;
    float2 blueOffset = -offset * dist * intensity;
    
    float2 redUV = input.TexC + redOffset;
    float2 blueUV = input.TexC + blueOffset;
    
    float4 originalColor = AfterLightMap.Sample(Sampler, input.TexC);
    
    float red = AfterLightMap.Sample(Sampler, redUV).r;
    float green = originalColor.g;
    float blue = AfterLightMap.Sample(Sampler, blueUV).b;
    float alpha = originalColor.a;
    
    return float4(red, green, blue, alpha);
}