#include "LightingUtil.hlsl"


Texture2D gHistory : register(t0);
Texture2D gCurrent : register(t1);
Texture2D gVelocity : register(t2);


SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearClamp : register(s3);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.TexC = float2(vid & 1, (vid & 2) >> 1);
    vout.PosH = float4(vout.TexC * float2(4, -4) + float2(-1, 1), 0, 1);

    return vout;
}
struct PSOutput
{
    float4 RT0 : SV_Target0;
};

float4 AdjustHDRColor(float4 color)
{
    //log
    return float4(color.rgb > 0.0 ? log(color.rgb) : -10.0, 1.0);
}


PSOutput PS(VertexOut pin)
{
    
    int x;
    int y;
    
    gCurrent.GetDimensions(x, y);
    int2 size = int2(x, y);
    float2 uv = pin.PosH.xy / size;
    PSOutput output;

    float4 currentColor = gCurrent.Load(int3(pin.PosH.xy, 0));

    float2 velocity = gVelocity.Load(int3(pin.PosH.xy, 0)).xy;

    float2 historyUV = uv - velocity;

    int2 historyPix = int2(historyUV);

    float4 historyColor = gHistory.Sample(gsamPointClamp, historyUV);
    float4 minColor = currentColor;
    float4 maxColor = minColor;

    for (int i = -1; i <= 1; i++)
        {
            for (int j = -1; j <= 1; j++)
            {
                //float4 color = gCurrent.Sample(gsamPointClamp, uv + 3*float2(i, j) / size);
                float4 color = gCurrent.Load(int3(pin.PosH.xy, 0) + int3(i, j, 0));
                minColor = min(minColor, color);
                maxColor = max(maxColor, color);

            }
        }
    float4 ClampedColor = clamp(historyColor, minColor, maxColor);
    
    float weightHistory = 0.9 * historyColor.a;
    float weightCurr = 0.1 * currentColor.a;

    
    
    
    
    
    
    //float4 blendedColor = ClampedColor * weightHistory + currentColor * weightCurr;
    float4 blendedColor = ClampedColor * 0.9 + currentColor * 0.1;
    
    output.RT0 = blendedColor;
    return output;
}
