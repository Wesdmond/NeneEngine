#include "GBuffer.hlsl"

float4 PS(PS_IN input) : SV_Target
{
    GBufferData buf = ReadGBuffer(input.TexC);
    float4 litColor = AfterLightMap.Sample(Sampler, input.TexC);
    
    return litColor;
}