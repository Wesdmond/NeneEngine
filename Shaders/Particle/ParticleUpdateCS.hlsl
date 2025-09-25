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

// Constant data that varies per material.
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
    float4 gAmbientLight;
};

float Random(float minV, float maxV)
{
    return clamp(sin(gTotalTime) * 43758.5453, minV, maxV);
}

static const float kShell = 0.10;
static const float kSlideFriction = 0.05;
void DodgeSphere(in float3 C, in float R, inout float3 pos, inout float3 vel)
{
    float3 v = pos - C;
    float d2 = dot(v, v);
    float Rinfl = R + kShell;

    if (d2 < Rinfl * Rinfl)
    {
        float d = sqrt(max(d2, 1e-8));
        float3 n = (d > 1e-6) ? (v / d) : float3(0, 1, 0);

        // Push out to sphere surface
        pos = C + n * Rinfl;

        // Remove velocity component toward sphere
        float vn = dot(vel, n);
        if (vn < 0.0)
            vel -= vn * n;

        // Apply sliding friction to tangential velocity
        float3 tang = vel - n * dot(vel, n);
        vel = tang * (1.0 - kSlideFriction) + n * max(0.0, dot(vel, n));
    }
}

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
        
        DodgeSphere(float3(0, 0, 0), 4.f, p.pos, p.vel);
        
        if (p.life < 0)
        {
            p.alive = 0;
            p.life = p.lifetime;
            p.vel = float3(0, -1.5, 0);
            p.pos.y = 40.5;
            //p.pos = float3(Random(-50, 50), 40.5, Random(-50, 50));
            p.alive = 1;
        }
    }
    gOut[i] = p;
}