struct Particle
{
    float3 pos;
    float3 beginPos;
    float3 vel;
    float life;
    float lifetime;
    float size;
    float rot;
    int alive;
    float4 color;
};

StructuredBuffer<Particle> gParticles : register(t0);

struct GSInput
{
    int vertexID : TEXCOORD0;
};

cbuffer Constants : register(b0)
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
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

GSInput VSMain(uint id : SV_VertexID)
{
    GSInput output;
    output.vertexID = id;
    return output;
}

[maxvertexcount(4)]
void GSMain(point GSInput input[1], inout TriangleStream<PSInput> output)
{
    uint particleIndex = input[0].vertexID;
    Particle p = gParticles[particleIndex];
    
    // Skip if particle is not alive
    if (p.alive == 0)
        return;

    float3 pos = p.pos;
    float4 color = p.color;
    float size = max(p.size, 0.01f);

    // Get camera vectors for billboarding
    float3 forward = normalize(gEyePosW - pos);
    float3 up = float3(0, 1, 0);
    
    // Handle case when forward is parallel to up
    if (abs(dot(forward, up)) > 0.99f)
        up = float3(0, 0, 1);
        
    float3 right = normalize(cross(up, forward));
    up = cross(forward, right);

    // Scale vectors by particle size
    right *= size;
    up *= size;

    PSInput vertex;
    vertex.color = color;

    // Create quad vertices
    float3 positions[4];
    positions[0] = pos - right - up; // bottom-left
    positions[1] = pos - right + up; // top-left  
    positions[2] = pos + right - up; // bottom-right
    positions[3] = pos + right + up; // top-right

    float2 uvs[4] =
    {
        float2(0, 1), // bottom-left
        float2(0, 0), // top-left
        float2(1, 1), // bottom-right  
        float2(1, 0)  // top-right
    };

    // Output quad vertices
    for (int i = 0; i < 4; i++)
    {
        vertex.pos = mul(float4(positions[i], 1.0), gViewProj);
        vertex.uv = uvs[i];
        output.Append(vertex);
    }
    
    output.RestartStrip();
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}