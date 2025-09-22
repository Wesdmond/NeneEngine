struct Particle
{
    float3 pos;
    float3 prevPos;
    float3 velocity;
    float3 acceleration;
    float energy;
    float size;
    float sizeDelta;
    float weight;
    float weightDelta;
    float4 color;
    float4 colorDelta;
};

cbuffer Constants : register(b0)
{
    float dt;
    float3 force;
};

ConsumeStructuredBuffer<Particle> gInput : register(u0);
AppendStructuredBuffer<Particle> gOutput : register(u1);

[numthreads(256, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    Particle p = gInput.Consume();

    p.energy -= dt;
    if (p.energy > 0.0f)
    {
        p.prevPos = p.pos;
        p.velocity += (p.acceleration + p.weight * force) * dt;
        p.pos += p.velocity * dt;
        p.size += p.sizeDelta * dt;
        p.weight += p.weightDelta * dt;
        p.color += p.colorDelta * dt;
        gOutput.Append(p);
    }
}