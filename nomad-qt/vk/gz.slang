// Interface do ImGui (gizmos): vertices em pixels, origem no canto superior esquerdo.
struct PushConst
{
    float2 scale;  // 2/largura, 2/altura
};
[[vk::push_constant]] ConstantBuffer<PushConst> pc;
[[vk::binding(0, 0)]] Sampler2D uTex;

struct VsIn
{
    [[vk::location(0)]] float2 aP : POSITION;
    [[vk::location(1)]] float2 aUV : TEXCOORD0;
    [[vk::location(2)]] float4 aC : COLOR0;
};
struct VsOut
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float2 vUV : TEXCOORD0;
    [[vk::location(1)]] float4 vC : COLOR0;
};

[shader("vertex")]
VsOut vsMain(VsIn i)
{
    VsOut o;
    o.pos = float4(i.aP * pc.scale - float2(1.0), 0.0, 1.0);
    o.vUV = i.aUV;
    o.vC = i.aC;
    return o;
}

[shader("fragment")]
float4 fsMain(VsOut v) : SV_Target
{
    return v.vC * uTex.Sample(v.vUV);
}
