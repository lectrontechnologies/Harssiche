// GTAO (Ground-Truth Ambient Occlusion, Jimenez et al. 2016), do renderizador NanoFarm.
// Escrito a partir do artigo; a forma geral (fatias, horizonte por passos, integral do arco) segue o XeGTAO (MIT) como referencia.
// Passos: [1] profundidade -> [fsGtao] visibilidade ruidosa (meia resolucao) -> [fsBlur] filtro que respeita a profundidade.
// Espaco de visao usado aqui: x direita, y para cima, z = distancia positiva a frente da camera.

struct AoPC
{
    float4 a;  // x,y = 1 / tamanho do alvo; z = raio (unidades da cena); w = potencia final
    float4 b;  // x,y = tan(fov/2) * (aspecto, 1); z = A = (f+n)/(n-f); w = B = 2fn/(n-f)
    float4 c;  // x,y = fator e soma do esmaecimento por distancia
};
[[vk::push_constant]] ConstantBuffer<AoPC> pc;
[[vk::binding(0, 0)]] Sampler2D uDepth;
[[vk::binding(1, 0)]] Sampler2D uAo;

struct VOut
{
    float4 pos : SV_Position;
};

[shader("vertex")]
VOut vsMain(uint vid : SV_VertexID)
{
    float2 p = float2(float((vid << 1) & 2u), float(vid & 2u));
    VOut o;
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

static const float PI = 3.14159265;
static const float HALF_PI = 1.57079633;

float linZ(float d)
{
    // profundidade de Vulkan (0..1) do projeto OpenGL do motor -> distancia positiva a camera
    return pc.b.w / (2.0 * d - 1.0 + pc.b.z);
}

float3 vpos(float2 uv, float l)
{
    float2 s = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    return float3(s * pc.b.xy * l, l);
}

float zAt(float2 uv)
{
    return linZ(uDepth.SampleLevel(uv, 0).x);
}

[shader("fragment")]
float fsGtao(float4 sp : SV_Position) : SV_Target
{
    float2 px = pc.a.xy;
    float2 uv = sp.xy * px;
    float d = uDepth.SampleLevel(uv, 0).x;
    if (d >= 0.99999) return 1.0;
    float Lc = linZ(d);
    float3 P = vpos(uv, Lc);

    // normal pela profundidade: usa o vizinho de menor salto em cada eixo
    float Ll = zAt(uv - float2(px.x, 0)), Lr = zAt(uv + float2(px.x, 0));
    float Lu = zAt(uv - float2(0, px.y)), Ld = zAt(uv + float2(0, px.y));
    float3 ex = abs(Lc - Ll) < abs(Lr - Lc) ? P - vpos(uv - float2(px.x, 0), Ll) : vpos(uv + float2(px.x, 0), Lr) - P;
    float3 ey = abs(Lc - Lu) < abs(Ld - Lc) ? vpos(uv - float2(0, px.y), Lu) - P : P - vpos(uv + float2(0, px.y), Ld);
    float3 N = normalize(cross(ex, ey));
    float3 V = normalize(-P);
    if (dot(N, V) < 0.0) N = -N;

    float noise = frac(52.9829189 * frac(dot(sp.xy, float2(0.06711056, 0.00583715))));
    float noise2 = frac(sp.x * 0.7548776662 + sp.y * 0.5698402910);
    float radPx = pc.a.z / (2.0 * Lc * pc.b.y) / px.y;
    radPx = clamp(radPx, 3.0, 0.2 / px.y);

    const int SLICES = 2;
    const int STEPS = 8;
    float vis = 0.0;
    for (int sl = 0; sl < SLICES; ++sl)
    {
        float phi = (float(sl) + noise) * PI / float(SLICES);
        float2 om = float2(cos(phi), sin(phi));  // direcao na tela (uv, y para baixo)
        float3 dirV = float3(om.x, -om.y, 0.0);  // mesma direcao no espaco de visao (y para cima)
        float3 ortho = dirV - dot(dirV, V) * V;
        float3 axis = normalize(cross(dirV, V));
        float3 projN = N - axis * dot(N, axis);
        float projLen = length(projN);
        float signN = sign(dot(ortho, projN));
        float cosN = saturate(dot(projN, V) / max(projLen, 1e-5));
        float n = signN * acos(cosN);

        float low0 = cos(n + HALF_PI), low1 = cos(n - HALF_PI);
        float h0 = low0, h1 = low1;
        for (int st = 0; st < STEPS; ++st)
        {
            float s = (float(st) + noise2) / float(STEPS);
            s = s * s + 0.002;
            float2 off = om * (s * radPx) * px;
            float2 uv0 = uv + off, uv1 = uv - off;
            float3 d0 = vpos(uv0, zAt(uv0)) - P;
            float3 d1 = vpos(uv1, zAt(uv1)) - P;
            float l0 = max(length(d0), 1e-4), l1 = max(length(d1), 1e-4);
            float c0 = lerp(low0, dot(d0, V) / l0, saturate(l0 * pc.c.x + pc.c.y));
            float c1 = lerp(low1, dot(d1, V) / l1, saturate(l1 * pc.c.x + pc.c.y));
            h0 = max(h0, c0);
            h1 = max(h1, c1);
        }
        float a0 = -acos(clamp(h1, -1.0, 1.0));
        float a1 = acos(clamp(h0, -1.0, 1.0));
        a0 = n + clamp(a0 - n, -HALF_PI, HALF_PI);
        a1 = n + clamp(a1 - n, -HALF_PI, HALF_PI);
        float arc0 = (cosN + 2.0 * a0 * sin(n) - cos(2.0 * a0 - n)) * 0.25;
        float arc1 = (cosN + 2.0 * a1 * sin(n) - cos(2.0 * a1 - n)) * 0.25;
        vis += projLen * (arc0 + arc1);
    }
    vis = saturate(vis / float(SLICES));
    return pow(vis, pc.a.w);
}

[shader("fragment")]
float fsBlur(float4 sp : SV_Position) : SV_Target
{
    float2 px = pc.a.xy;
    float2 uv = sp.xy * px;
    float d = uDepth.SampleLevel(uv, 0).x;
    if (d >= 0.99999) return 1.0;
    float Lc = linZ(d);
    float sum = 0.0, ws = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
        {
            float2 u2 = uv + float2(float(x), float(y)) * px;
            float w = saturate(1.0 - abs(zAt(u2) - Lc) / (Lc * 0.04 + 1e-3));
            sum += uAo.SampleLevel(u2, 0).x * w;
            ws += w;
        }
    return sum / max(ws, 1e-4);
}
