// Constant data that varies per material.
cbuffer cbPass : register(b0)
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
    float4 gAmbientLight;
    
    float3 gFogColor;
    float gGlobalDensity;
    float gHeightFalloff;
    float gBaseHeight;
    float gFogAnisotropy;
    float gSunIntensity;
    float3 gSunDirection;
    float pad0;
};

struct PS_IN
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

PS_IN VS(uint vid : SV_VertexID)
{
    PS_IN output = (PS_IN) 0;
    
    float2 texcoord = float2((vid << 1) & 2, vid & 2);
    output.PosH = float4(texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.TexC = texcoord;
    
    return output;
}

SamplerState Sampler : register(s0);

Texture2D DiffuseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D DepthMap : register(t3);
Texture2D AfterLightMap : register(t4);
    
struct GBufferData
{
    float4 Albedo;
    float3 Normal;
    float3 Roughness;
    float3 Depth;
};

GBufferData ReadGBuffer(float2 texC)
{
    GBufferData buf = (GBufferData) 0;
    
    buf.Albedo = DiffuseMap.Sample(Sampler, texC);
    buf.Normal = NormalMap.Sample(Sampler, texC);
    buf.Roughness = RoughnessMap.Sample(Sampler, texC);
    buf.Depth = DepthMap.Sample(Sampler, texC);
    
    return buf;
}



float PhaseHG(float mu, float g)
{
    float g2 = g * g;
    float denom = pow(1.0f + g2 - 2.0f * g * mu, 1.5f);
    return (1.0f - g2) / max(denom, 1e-3f);
}

float PhaseRayleigh(float mu)
{
    return (1.0f + mu * mu);
}

float4 PS(PS_IN pin) : SV_TARGET
{
    float2 texC = pin.TexC;
    float3 sceneColor = AfterLightMap.Sample(Sampler, texC).rgb;
    float depth = DepthMap.Sample(Sampler, texC).r;

    float2 ndc;
    ndc.x = pin.TexC.x * 2.0f - 1.0f;
    ndc.y = (1.0f - pin.TexC.y) * 2.0f - 1.0f;

    // ray to farplane
    float4 clipFar = float4(ndc.x, ndc.y, 1.0f, 1.0f);
    float4 worldFarH = mul(clipFar, gInvViewProj);
    worldFarH /= worldFarH.w;
    float3 rayDir = normalize(worldFarH.xyz - gEyePosW);

    // calc distance
    bool hasGeometry = (depth < 1.0f - 1e-5f);
    float maxDist;

    if (hasGeometry)
    {
        float4 clipPos = float4(ndc.x, ndc.y, depth, 1.0f);
        float4 worldPosH = mul(clipPos, gInvViewProj);
        worldPosH /= worldPosH.w;
        float3 worldPos = worldPosH.xyz;
        maxDist = length(worldPos - gEyePosW);
    }
    else
    {
        maxDist = gFarZ;
    }

    maxDist = max(maxDist, 1e-3f);

    float lambda = gHeightFalloff;
    float k = gGlobalDensity;

    float y0 = gEyePosW.y - gBaseHeight;
    float y1 = y0 + rayDir.y * maxDist;

    float tau;
    if (abs(rayDir.y) < 1e-3f)
    {
        float density = k * exp(-lambda * y0);
        tau = density * maxDist;
    }
    else
    {
        float exp0 = exp(-lambda * y0);
        float exp1 = exp(-lambda * y1);
        tau = (k / (lambda * rayDir.y)) * (exp0 - exp1);
        tau = abs(tau);
    }

    tau = clamp(tau, 0.0f, 50.0f);
    float T = exp(-tau); // transmittance

    // in-scattering

    float3 sunDir = normalize(gSunDirection);
    float mu = dot(-rayDir, sunDir); // angle(cam, sun)

    float rayleighPhase = PhaseRayleigh(mu);
    float miePhase = PhaseHG(mu, gFogAnisotropy);

    // -1-1 to 0-1
    float sunHeight = saturate(sunDir.y * 0.5f + 0.5f);

    // Rayleigh
    float3 rayleighHorizon = float3(1.0f, 0.55f, 0.2f);
    float3 rayleighZenith = gFogColor; // base color
    float3 rayleighColor = lerp(rayleighHorizon, rayleighZenith, sunHeight);

    // Mie
    float3 mieSunset = float3(1.0f, 0.7f, 0.3f);
    float3 mieDay = float3(1.0f, 0.9f, 0.7f);
    float3 mieColor = lerp(mieSunset, mieDay, sunHeight);

    float rayleighWeight = 1.0f;
    float mieWeight = 0.4f;

    float3 scatterColor =
        rayleighColor * rayleighPhase * rayleighWeight +
        mieColor * miePhase * mieWeight;

    float3 fogLight = scatterColor * gSunIntensity;

    // blend
    float3 fogContribution = fogLight * (1.0f - T);
    float3 color = sceneColor * T + fogContribution;

    // tonemap gamma
    color = color / (1.0f + color);
    color = pow(color, 1.0f / 2.2f);

    return float4(color, 1.0f);
}