// Malhas do motor (Slang -> SPIR-V, Vulkan 1.1).
// Vertices: posicao + normal; opcionalmente osso (indices e pesos) e uv. Os dados por objeto (matrizes, cor,
// parametros) vem de um buffer de armazenamento indexado por SV_VulkanInstanceID (instancia + firstInstance) (= indice da ordem de desenho).
// Matrizes em layout de coluna (como as do motor, que vieram do OpenGL): compilar com -matrix-layout-column-major.

struct Op
{
    float4x4 mvp;
    float4x4 mod;
    float4 col;
    float4 a;   // a.x = lit, a.yzw = escala (retangulos)
    float4 b;   // b.xy = metal, rugosidade; b.z = esqueleto ligado; b.w = primeira matriz da paleta
    float4 tl;  // camadas de textura: base, metal/rugosidade, normal; w = recorte por alpha
};

struct PushConst
{
    float4 cam;  // xyz = posicao da camera, w = matcap ligado
    float4 mcR;  // base do matcap; w = altura da tela
    float4 mcU;
    float4 mcB;
};

[[vk::binding(0, 0)]] StructuredBuffer<Op> ops;
[[vk::binding(1, 0)]] StructuredBuffer<float4x4> pal;
[[vk::binding(0, 1)]] Sampler2DArray uTexA;
[[vk::push_constant]] ConstantBuffer<PushConst> pc;

struct VsIn
{
    [[vk::location(0)]] float3 aP : POSITION;
    [[vk::location(1)]] float3 aN : NORMAL;
    [[vk::location(2)]] float4 aJ : BLENDINDICES;  // so em malhas com esqueleto
    [[vk::location(3)]] float4 aW : BLENDWEIGHT;
    [[vk::location(4)]] float2 aUV : TEXCOORD0;    // so em malhas com coordenadas de textura
    uint inst : SV_VulkanInstanceID;
};

struct VsOut
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float3 vN : TEXCOORD0;
    [[vk::location(1)]] float3 vP : TEXCOORD1;
    [[vk::location(2)]] float3 vW : TEXCOORD2;
    [[vk::location(3)]] nointerpolation int vOp : TEXCOORD3;
    [[vk::location(4)]] float2 vUV : TEXCOORD4;
};

[shader("vertex")]
VsOut vsMain(VsIn i)
{
    Op o = ops[i.inst];
    float4 p = float4(i.aP, 1.0);
    float3 n = i.aN;
    if (o.b.z > 0.5)
    {
        // skinning: mesma formula do motor (a matriz do objeto e identidade nesse caso)
        int base = int(o.b.w);
        float4x4 S = i.aW.x * pal[base + int(i.aJ.x)] + i.aW.y * pal[base + int(i.aJ.y)]
                   + i.aW.z * pal[base + int(i.aJ.z)] + i.aW.w * pal[base + int(i.aJ.w)];
        p = mul(S, p);
        n = mul((float3x3)S, n);
    }
    float4 c = mul(o.mvp, p);
    // as matrizes do motor sao de OpenGL (y para cima, z de -w a w): converte para o clip do Vulkan
    c.y = -c.y;
    c.z = (c.z + c.w) * 0.5;

    VsOut r;
    r.pos = c;
    r.vN = mul((float3x3)o.mod, n);
    r.vP = i.aP;
    r.vW = mul(o.mod, p).xyz;
    r.vOp = int(i.inst);
    r.vUV = i.aUV;
    return r;
}

float3 shade(float3 L, float3 Lc, float3 N, float3 V, float3 alb, float3 F0, float met, float rou)
{
    float3 H = normalize(L + V);
    float a = rou * rou, a2 = a * a;
    float NL = max(dot(N, L), 0.0), NV = max(dot(N, V), 1e-3), NH = max(dot(N, H), 0.0), VH = max(dot(V, H), 0.0);
    float dd = NH * NH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * dd * dd + 1e-6);
    float k = (rou + 1.0) * (rou + 1.0) / 8.0;
    float G = (NL / (NL * (1.0 - k) + k)) * (NV / (NV * (1.0 - k) + k));
    float3 F = F0 + (1.0 - F0) * pow(max(1.0 - VH, 0.0), 5.0);
    float3 spec = D * G * F / (4.0 * NV * max(NL, 1e-3));
    float3 kd = (1.0 - F) * (1.0 - met);
    return (kd * alb / 3.14159 + spec) * Lc * NL;
}

float3 pbr(VsOut v, float4 uC, float2 uMR, float4 tl)
{
    float3 N = normalize(v.vN), V = normalize(pc.cam.xyz - v.vW);
    if (dot(N, V) < 0.0) N = -N;
    float3 base = uC.rgb;
    float met = uMR.x, rou = uMR.y;
    if (tl.x >= 0.0) base *= uTexA.Sample(float3(v.vUV, tl.x)).rgb;
    if (tl.y >= 0.0)
    {
        float4 mrs = uTexA.Sample(float3(v.vUV, tl.y));
        rou *= mrs.g;
        met *= mrs.b;
    }
    if (tl.z >= 0.0)
    {
        // mapa de normais com tangente derivada das coordenadas de textura
        float3 tn = uTexA.Sample(float3(v.vUV, tl.z)).xyz * 2.0 - 1.0;
        float3 dp1 = ddx(v.vW), dp2 = ddy(v.vW);
        float2 du1 = ddx(v.vUV), du2 = ddy(v.vUV);
        float3 dp2p = cross(dp2, N), dp1p = cross(N, dp1);
        float3 Tt = dp2p * du1.x + dp1p * du2.x, Bt = -(dp2p * du1.y + dp1p * du2.y);
        float iv = rsqrt(max(dot(Tt, Tt), dot(Bt, Bt)) + 1e-20);
        float3x3 TBN = float3x3(Tt * iv, Bt * iv, N);  // linhas = T, B, N
        N = normalize(mul(tn, TBN));                      // = TBN^T * tn, como mat3(T,B,N) * tn no GLSL
    }
    float3 alb = pow(base, float3(2.2));
    rou = clamp(rou, 0.05, 1.0);
    float3 F0 = lerp(float3(0.04), alb, met);
    float3 col = shade(normalize(float3(.4, .8, .5)), float3(3.2, 3.0, 2.8), N, V, alb, F0, met, rou)
               + shade(normalize(float3(-.6, .3, -.4)), float3(.6, .7, 1.0), N, V, alb, F0, met, rou);
    float3 amb = lerp(float3(.12, .11, .10), float3(.22, .26, .34), N.y * .5 + .5);
    float NV = max(dot(N, V), 0.0);
    float3 Fa = F0 + (max(float3(1.0 - rou), F0) - F0) * pow(1.0 - NV, 5.0);
    col += amb * (alb * (1.0 - met) + Fa * 0.6);
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14);
    return pow(clamp(col, 0.0, 1.0), float3(1.0 / 2.2));
}

float3 matcap(VsOut v)
{
    float3 N = normalize(v.vN), V = normalize(pc.cam.xyz - v.vW);
    if (dot(N, V) < 0.0) N = -N;
    float3 n = float3(dot(N, pc.mcR.xyz), dot(N, pc.mcU.xyz), dot(N, pc.mcB.xyz));
    float3 L1 = normalize(float3(.10, .50, .85)), L2 = normalize(float3(-.60, -.30, .50));
    float d1 = max(dot(n, L1), 0.0), d2 = max(dot(n, L2), 0.0);
    float amb = .46 + .10 * n.y;
    float3 c = float3(.64) * (amb + .50 * d1 + .22 * d2);
    float3 H = normalize(L1 + float3(0.0, 0.0, 1.0));
    c += float3(.04) * pow(max(dot(n, H), 0.0), 24.0);
    return clamp(c, 0.0, 1.0);
}

[shader("fragment")]
float4 fsMain(VsOut v) : SV_Target
{
    Op op = ops[v.vOp];
    float uL = op.a.x;
    float3 uS = op.a.yzw;
    float4 uC = op.col;
    if (uL < -2.5)
    {
        // fundo do viewport (gradiente vertical)
        float t = clamp(1.0 - v.pos.y / max(pc.mcR.w, 1.0), 0.0, 1.0);
        float3 top = float3(.49, .56, .64), bot = float3(.065, .07, .08);
        return float4(lerp(bot, top, t), 1.0);
    }
    if (uL < -1.5)
    {
        // losango
        float2 q = (v.vP.xy - .5) * uS.xy;
        float dd = (abs(q.x) + abs(q.y) - uS.x * .5) * .7071;
        return float4(uC.rgb, uC.a * clamp(.5 - dd, 0.0, 1.0));
    }
    if (uL < 0.0)
    {
        // retangulo arredondado
        float2 q = (v.vP.xy - .5) * uS.xy;
        float2 e = abs(q) - uS.xy * .5 + uS.z;
        float dd = length(max(e, 0.0)) + min(max(e.x, e.y), 0.0) - uS.z;
        return float4(uC.rgb, uC.a * clamp(.5 - dd, 0.0, 1.0));
    }
    if (uL > 0.5)
    {
        // malha sombreada
        if (op.tl.w > 0.5 && op.tl.x >= 0.0 && uTexA.Sample(float3(v.vUV, op.tl.x)).a < 0.5) discard;
        return float4(pc.cam.w > 0.5 ? matcap(v) : pbr(v, uC, op.b.xy, op.tl), uC.a);
    }
    return float4(uC.rgb, uC.a);
}
