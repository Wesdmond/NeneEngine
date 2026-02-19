

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvWorld;
    float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};
cbuffer cbTerrainTile : register(b3)
{
    float3 gTilePosition;
    float gTileSize;
    
    float mapSize;
    float heightScale;
    float2 padding1;
    
    float2 gHitUV; // <--- ДОБАВЛЕНО
    float gBrushRadius; // <--- ДОБАВЛЕНО
    float gBrushActive;
    float padding2;

};
// Texture resources
Texture2D gHeightMap : register(t0);
Texture2D gDiffuseMap : register(t1);
Texture2D gNormalMap : register(t2); 
Texture2D gHeightModificationMap : register(t3); // Используем t3

SamplerState gSamPointWrap : register(s0);
SamplerState gSamPointClamp : register(s1);
SamplerState gSamLinearWrap : register(s2);
SamplerState gSamLinearClamp : register(s3);
SamplerState gSamAnisotropicWrap : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
    float2 TexCl : TEXCOORD2;
};

struct PixelOut
{
    float4 Albedo : SV_Target0; 
    float4 Normal : SV_Target1;
    float4 Position : SV_Target2;
};

// Константы для terrain
static const float TEXTURE_REPEAT = 3.0f;
static const float NORMAL_SAMPLE_OFFSET = 0.01f; 

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
     float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;
    float coeff = gTileSize / mapSize ;
    vout.TexC *= coeff;
    vout.TexC += gTilePosition.xz / mapSize;
    vout.TexCl = vin.TexC;

    
    float baseHeight = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC, 0).r;
    float heightMod = gHeightModificationMap.SampleLevel(gSamLinearClamp, vout.TexC, 0).r;
    float finalHeight = baseHeight + heightMod;
    
    // Ограничиваем значение
    finalHeight = clamp(finalHeight, 0.0, 1.0);
    //float height = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC, 0).r;

    
    if (gBrushActive != 0.0f)
    {
        float dist = distance(vout.TexC, gHitUV);
        
        if (dist < gBrushRadius)
        {
            float falloff = 1.0 - (dist / gBrushRadius);
            falloff = falloff * falloff;
            
            // ВАЖНО: проверяем знак gBrushActive
            if (gBrushActive > 0.0f)
            {
                // Поднимаем террейн
                finalHeight += 0.1f * falloff;

            }
            else if (gBrushActive < 0.0f)
            {
                // Опускаем террейн
                finalHeight -= 0.1f * falloff;
            }
            
            finalHeight = clamp(finalHeight, 0.0, 1.0);
        }
    }

    float3 posL = vin.PosL;
    posL.y = posL.y + finalHeight * heightScale;
    

    float4 posW = mul(float4(posL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    

    float2 texelSize = float2(1.0f / mapSize, 1.0f / mapSize);
    
 
    float hL = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC + float2(-texelSize.x, 0.0f), 0).r;
    float hR = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC + float2(texelSize.x, 0.0f), 0).r;
    float hD = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC + float2(0.0f, -texelSize.y), 0).r;
    float hU = gHeightMap.SampleLevel(gSamLinearClamp, vout.TexC + float2(0.0f, texelSize.y), 0).r;
    

    float dX = (hR - hL) * heightScale;
    float dZ = (hU - hD) * heightScale; 
    

    float3 normal = normalize(float3(-dX, 1.0f, -dZ));
    

    float3 tangent = normalize(float3(1.0f, dX, 0.0f));
    

    vout.NormalW = normalize(mul(normal, (float3x3) gWorld));
    vout.TangentW = normalize(mul(tangent, (float3x3) gWorld));
    

    vout.PosH = mul(posW, gViewProj);
    
    return vout;
}
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{

    float3 normalT = 2.0f * normalMapSample - 1.0f;
    

    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);

    float3 bumpedNormalW = mul(normalT, TBN);
    
    return bumpedNormalW;
}

PixelOut PS(VertexOut pin) : SV_Target
{
    PixelOut pout;
    
    if (gBrushActive > 0.5f || gBrushActive < -0.5f)
    {
        float dist = distance(pin.TexC, gHitUV);
        
        if (dist < gBrushRadius)
        {
            float intensity = 1.0 - (dist / gBrushRadius);
            
            // Получаем оригинальный цвет текстуры
            float4 diffuseAlbedo = gDiffuseMap.Sample(gSamAnisotropicWrap, pin.TexC);
            diffuseAlbedo *= gDiffuseAlbedo;
            
            // Выбираем цвет кисти
            float4 brushColor = float4(1, 0, 0, 0.5f); 
            if (gBrushActive < -0.5f)
                brushColor = float4(0, 0, 1, 0.5f);
            
            // Смешиваем с текстурой
            diffuseAlbedo.rgb = lerp(diffuseAlbedo.rgb, brushColor.rgb, brushColor.a * intensity);
            
            pout.Albedo = diffuseAlbedo;
            
            // Нормали и позиция
            float3 normalMapSample = gNormalMap.Sample(gSamAnisotropicWrap, pin.TexC).rgb;
            pin.NormalW = normalize(pin.NormalW);
            pin.TangentW = normalize(pin.TangentW);
            float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
            
            pout.Normal = float4(bumpedNormalW, gRoughness);
            pout.Position = float4(pin.PosW, 1.0f);
            return pout;
        }
    }
    
    float4 diffuseAlbedo = gDiffuseMap.Sample(gSamAnisotropicWrap, pin.TexC);
    diffuseAlbedo *= gDiffuseAlbedo;
    
  
    float3 normalMapSample = gNormalMap.Sample(gSamAnisotropicWrap, pin.TexC).rgb;
    

    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW);
    

    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);

    
    
    pout.Albedo = diffuseAlbedo;
    pout.Normal = float4(bumpedNormalW, gRoughness);
    pout.Position = float4(pin.PosW, 1.0f);
    
    return pout;
}
