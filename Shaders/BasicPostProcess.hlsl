#include "GBuffer.hlsl"

float4 PS(PS_IN input) : SV_Target
{
    float2 localUV = frac(input.TexC * 2);
    GBufferData buf = ReadGBuffer(localUV);
    
    
    if (input.TexC.x > 0.5 && input.TexC.y > 0.5)
        return float4(buf.Normal, 1);
    if (input.TexC.x > 0.5 && input.TexC.y < 0.5)
        return buf.Albedo;
    if (input.TexC.x < 0.5 && input.TexC.y > 0.5)
        return AfterLightMap.Sample(Sampler, localUV);
    if (input.TexC.x < 0.5 && input.TexC.y < 0.5)
    {
        float depth = buf.Depth.r;
        return float4(depth, depth, depth, 1);
    }
    
    return float4(0.f, 0.f, 0.f, 0.f);

}