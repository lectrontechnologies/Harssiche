// Texto do motor: vertices (x, y em px; u, v) em um buffer sequencial; cor e tamanho da tela vem por push constants.
struct PushConst
{
    float2 scr;
    float2 pad;
    float4 col;
};
[[vk::push_constant]] ConstantBuffer<PushConst> pc;
[[vk::binding(0, 0)]] Sampler2D uTex;

struct VsIn
{
    [[vk::location(0)]] float4 aV : POSITION;
};
struct VsOut
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float2 vT : TEXCOORD0;
};

[shader("vertex")]
VsOut vsMain(VsIn i)
{
    VsOut o;
    o.pos = float4(i.aV.x / pc.scr.x * 2.0 - 1.0, i.aV.y / pc.scr.y * 2.0 - 1.0, 0.0, 1.0);
    o.vT = i.aV.zw;
    return o;
}

[shader("fragment")]
float4 fsMain(VsOut v) : SV_Target
{
    return float4(pc.col.rgb, pc.col.a * uTex.Sample(v.vT).r);
}
