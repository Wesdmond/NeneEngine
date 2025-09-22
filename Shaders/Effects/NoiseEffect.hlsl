#include "GBuffer.hlsl"

float Random(float2 uv, float seed)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233)) + seed) * 43758.5453);
}

float4 PS(PS_IN input) : SV_Target
{
    GBufferData buf = ReadGBuffer(input.TexC);
    float4 litColor = AfterLightMap.Sample(Sampler, input.TexC);
    
    float grainIntensity = 0.1;
    
    float noise = Random(input.TexC * gRenderTargetSize, gTotalTime);

    float3 grain = noise * grainIntensity;
    return float4(litColor.rgb + grain, litColor.a);
}