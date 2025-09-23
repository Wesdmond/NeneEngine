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

// Входной буфер для вершинного шейдера
StructuredBuffer<Particle> gParticles : register(t0);

// Выход вершинного шейдера (вход геометрического шейдера)
struct GSInput
{
    float3 pos : POSITION;
    float4 color : COLOR;
    float size : SIZE;
};

// Константы для рендеринга
cbuffer Constants : register(b0)
{
    float4x4 viewProj;
};

// Выход геометрического шейдера (вход пиксельного шейдера)
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
    vertex.pos = mul(float4(pos - right * size + up * size, 1.0), viewProj);
    vertex.uv = float2(0, 0);
    output.Append(vertex);

    // Левый верхний угол
    vertex.pos = mul(float4(pos - right * size - up * size, 1.0), viewProj);
    vertex.uv = float2(0, 1);
    output.Append(vertex);

    // Правый нижний угол
    vertex.pos = mul(float4(pos + right * size + up * size, 1.0), viewProj);
    vertex.uv = float2(1, 0);
    output.Append(vertex);

    // Правый верхний угол
    vertex.pos = mul(float4(pos + right * size - up * size, 1.0), viewProj);
    vertex.uv = float2(1, 1);
    output.Append(vertex);

    output.RestartStrip();
}

// Пиксельный шейдер
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}