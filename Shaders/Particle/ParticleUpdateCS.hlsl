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

// Shaders/ParticlesCS.hlsl (the last image shader)
StructuredBuffer<Particle> gin : register(t0);
RWStructuredBuffer<Particle> gOut : register(u0);

cbuffer SimCB : register(b0)
{
    float dt;
    float3 gravity;
};

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = gin[i];
    if (p.alive != 0)
    {
        p.vel += gravity * dt;
        p.pos += p.vel * dt;
        p.life -= dt;
        
        if (p.life < 0)
        {
            p.alive = 0;
            p.life = p.lifetime;
            p.vel = float3(0, -1.5, 0);
            p.pos.y = 40.5;
            p.alive = 1;
        }
    }
    gOut[i] = p;
}