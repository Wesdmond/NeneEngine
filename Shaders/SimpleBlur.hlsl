#include "GBuffer.hlsl"

static const float gBlurWeights[3] = { 0.227027, 0.316216, 0.070270 }; // Center, neighbours, angles
static const int gBlurRadius = 2; // Half core size

float4 ApplyGaussianBlur(float2 texC, float intensity)
{
    // Pixel size in UV-space
    float2 pixelSize = gInvRenderTargetSize;
    
    float4 blurredColor = 0.0;
    float totalWeight = 0.0;
    
    // Core 5x5
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        for (int j = -gBlurRadius; j <= gBlurRadius; ++j)
        {
            float weight = gBlurWeights[abs(i)] * gBlurWeights[abs(j)];
            
            float2 offset = float2(i, j) * pixelSize * intensity;
            
            float4 sampleColor = AfterLightMap.Sample(Sampler, texC + offset);
            
            blurredColor += sampleColor * weight;
            totalWeight += weight;
        }
    }
    
    blurredColor /= totalWeight;
    
    return blurredColor;
}

float4 PS(PS_IN input) : SV_Target
{
    GBufferData buf = ReadGBuffer(input.TexC);
    float4 litColor = AfterLightMap.Sample(Sampler, input.TexC);

    float blurIntensity = 2;
    litColor = ApplyGaussianBlur(input.TexC, blurIntensity);
    
    return litColor;
}