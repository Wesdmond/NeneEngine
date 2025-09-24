// Общая структура частицы
//struct Particle
//{
//    float3 pos;
//    float3 prevPos;
//    float3 velocity;
//    float3 acceleration;
//    float energy;
//    float size;
//    float sizeDelta;
//    float weight;
//    float weightDelta;
//    float4 color;
//    float4 colorDelta;
//};

struct Particle
{
    float3 pos;
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
    float3 pos : POSITION;
    float4 color : COLOR;
    float size : SIZE;
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

// Вершинный шейдер
GSInput VSMain(uint id : SV_VertexID)
{
    Particle p = gParticles[id];
    GSInput output;
    output.pos = p.pos;
    output.color = p.color;
    output.size = p.size;
    return output;
}

// Геометрический шейдер
[maxvertexcount(4)]
void GSMain(point GSInput input[1], inout TriangleStream<PSInput> output)
{
    float3 pos = input[0].pos;
    float4 color = input[0].color;
    float size = input[0].size;

    float3 up = float3(0, 1, 0);
    float3 right = float3(1, 0, 0); // Упрощенно, можно использовать view матрицу для ориентации

    PSInput vertex;
    vertex.color = color;

    // Левый нижний угол
    vertex.pos = mul(float4(pos - right * size + up * size, 1.0), gViewProj);
    vertex.uv = float2(0, 0);
    output.Append(vertex);

    // Левый верхний угол
    vertex.pos = mul(float4(pos - right * size - up * size, 1.0), gViewProj);
    vertex.uv = float2(0, 1);
    output.Append(vertex);

    // Правый нижний угол
    vertex.pos = mul(float4(pos + right * size + up * size, 1.0), gViewProj);
    vertex.uv = float2(1, 0);
    output.Append(vertex);

    // Правый верхний угол
    vertex.pos = mul(float4(pos + right * size - up * size, 1.0), gViewProj);
    vertex.uv = float2(1, 1);
    output.Append(vertex);

    output.RestartStrip();
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}