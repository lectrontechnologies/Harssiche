// Parallax: path tracer em compute (nucleo da Etapa 3)
// cena: triangulos + BVH binaria (construida na CPU); sol com sombra suave + ceu; material difuso + GGX (metal/rugosidade)
struct PC {
    float3 eye;
    float tanY;
    float3 right;
    float aspect;
    float3 up;
    int ntex;      // camadas validas no array de texturas (0 = sem texturas)
    float3 fwd;
    int dn;        // 1 = grava normal/profundidade/albedo e o resolve usa o resultado do filtro
    int w;
    int h;
    int y0;
    int rows;
    int sample;
    int bounces;
    int night;
    int ntri;
    int src;       // filtro: buffer de leitura (0/1)
    int stp;       // filtro: distancia do passo a-trous (1, 2, 4, 8, 16)
    float phi;     // filtro: tolerancia de luminancia
    int tonemap;     // 0 = suave (shoulder), 1 = ACES
    int envOn;       // 1 = ambiente vem do HDRI (bEnv/bEnvD)
    float envScale;
    int glass;       // 1 = ha vidro na cena: a sombra atravessa a transmissao
    float expo;      // multiplicador de exposicao (2^EV)
};
[[vk::push_constant]] ConstantBuffer<PC> pc;

[[vk::binding(0, 0)]] StructuredBuffer<float4> bPos;    // 3 float4 por triangulo (p0, p1, p2)
[[vk::binding(1, 0)]] StructuredBuffer<float4> bShade;  // 4 float4: n0, n1, n2 (w de n2 = metal), cor (w = rugosidade)
[[vk::binding(2, 0)]] StructuredBuffer<float4> bNode;   // 2 float4 por no: (min, esq/primeiro), (max, quantidade)
[[vk::binding(3, 0)]] StructuredBuffer<float4> bTexI;   // 3 float4 por triangulo: (u0,v0,u1,v1), (u2,v2,base,metal/rug), (normal,recorte,0,0)
// acumulo: dn == 0 -> 1 float4 por pixel (soma rgb, contagem a); dn == 1 -> 3 por pixel: [soma rgb|contagem], [soma normal|soma profundidade], [soma albedo|contagem]
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> bAcc;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> bTmp;  // filtro: 2 float4 por pixel (ping-pong de iluminacao desmodulada pelo albedo)
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> bOut;    // RGBA8 final, de cima para baixo
[[vk::binding(7, 0)]] Sampler2DArray uTex;
[[vk::binding(8, 0)]] StructuredBuffer<float> bEnvD;    // distribuicao 2D do HDRI por luminancia (formato do InfiniteLight do LuxCore)
[[vk::binding(9, 0)]] StructuredBuffer<float4> bEnv;    // HDRI equirretangular (Y para cima), 1 float4 por texel
              // texturas de material (camadas size x size RGBA8)

static const float PI = 3.14159265;

uint pcg(uint v)
{
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint st)
{
    st = pcg(st);
    return float(st >> 8) * (1.0 / 16777216.0);
}

float sinv(float x)
{
    return 1.0 / (abs(x) < 1e-8 ? (x < 0.0 ? -1e-8 : 1e-8) : x);
}

bool hitTri(float3 o, float3 d, int ti, inout float tmax, out float bu, out float bv)
{
    bu = 0.0;
    bv = 0.0;
    float3 p0 = bPos[ti * 3].xyz;
    float3 e1 = bPos[ti * 3 + 1].xyz - p0;
    float3 e2 = bPos[ti * 3 + 2].xyz - p0;
    float3 pv = cross(d, e2);
    float det = dot(e1, pv);
    if (abs(det) < 1e-12) return false;
    float inv = 1.0 / det;
    float3 tv = o - p0;
    float u = dot(tv, pv) * inv;
    if (u < 0.0 || u > 1.0) return false;
    float3 qv = cross(tv, e1);
    float v = dot(d, qv) * inv;
    if (v < 0.0 || u + v > 1.0) return false;
    float t = dot(e2, qv) * inv;
    if (t <= 1e-6 || t >= tmax) return false;
    tmax = t;
    bu = u;
    bv = v;
    return true;
}

bool boxHit(int ni, float3 o, float3 inv, float tmax, out float tn)
{
    float4 a = bNode[ni * 2];
    float4 b = bNode[ni * 2 + 1];
    float3 t0 = (a.xyz - o) * inv;
    float3 t1 = (b.xyz - o) * inv;
    float3 tmn = min(t0, t1);
    float3 tmx = max(t0, t1);
    tn = max(max(tmn.x, tmn.y), max(tmn.z, 0.0));
    float tf = min(min(tmx.x, tmx.y), min(tmx.z, tmax));
    return tn <= tf;
}

// triangulo com recorte por alpha: o buraco da textura nao bloqueia o raio
bool alphaOk(int ti, float hu, float hv)
{
    float4 tC = bTexI[ti * 3 + 2];
    if (tC.y <= 0.5 || pc.ntex <= 0) return true;
    float4 tB = bTexI[ti * 3 + 1];
    int lay = int(tB.z);
    if (lay < 0 || lay >= pc.ntex) return true;
    float4 tA = bTexI[ti * 3];
    float2 uv = (1.0 - hu - hv) * tA.xy + hu * tA.zw + hv * tB.xy;
    return uTex.SampleLevel(float3(uv, float(lay)), 0.0).a >= 0.5;
}

// percorre a BVH visitando primeiro o filho mais proximo; anyHit = so quer saber se algo bloqueia (sombra)
bool traverse(float3 o, float3 d, inout float tmax, out int hit, out float hu, out float hv, bool anyHit)
{
    float3 inv = float3(sinv(d.x), sinv(d.y), sinv(d.z));
    int stack[64];
    float stT[64];
    int sp = 0;
    int node = 0;
    hit = -1;
    hu = 0.0;
    hv = 0.0;
    float tr;
    if (!boxHit(0, o, inv, tmax, tr)) return false;
    while (true) {
        float4 a = bNode[node * 2];
        int cnt = asint(bNode[node * 2 + 1].w);
        bool pop = true;
        if (cnt > 0) {
            int first = asint(a.w);
            for (int i = 0; i < cnt; i++) {
                float u, v;
                float tm0 = tmax;
                if (hitTri(o, d, first + i, tmax, u, v)) {
                    if (!alphaOk(first + i, u, v)) {
                        tmax = tm0;
                        continue;
                    }
                    hit = first + i;
                    hu = u;
                    hv = v;
                    if (anyHit) return true;
                }
            }
        } else {
            int l = node + 1;
            int r = asint(a.w);
            float dl, dr;
            bool hl = boxHit(l, o, inv, tmax, dl);
            bool hr = boxHit(r, o, inv, tmax, dr);
            if (hl && hr) {
                bool lf = dl <= dr;
                if (sp < 63) {
                    stack[sp] = lf ? r : l;
                    stT[sp] = lf ? dr : dl;
                    sp++;
                }
                node = lf ? l : r;
                pop = false;
            } else if (hl) {
                node = l;
                pop = false;
            } else if (hr) {
                node = r;
                pop = false;
            }
        }
        if (pop) {
            bool got = false;
            while (sp > 0) {
                sp--;
                if (stT[sp] <= tmax) {
                    node = stack[sp];
                    got = true;
                    break;
                }
            }
            if (!got) break;
        }
    }
    return hit >= 0;
}

float3 sunDir()
{
    return normalize(float3(0.45, 0.75, 0.35));
}
float3 sunIrr(int night)
{
    return night != 0 ? float3(0.10, 0.13, 0.22) : float3(1.7, 1.6, 1.45);
}
float3 skyRad(float3 d, int night)
{
    float t = saturate(d.y);
    if (night != 0) {
        float3 s = lerp(float3(0.020, 0.028, 0.050), float3(0.004, 0.007, 0.020), sqrt(t));
        return d.y < 0.0 ? s * 0.5 : s;
    }
    // ceu mais suave (menos saturado) e abaixo do horizonte segue a cor do horizonte, sem faixa escura
    float3 hor = float3(0.78, 0.83, 0.92);
    float3 zen = float3(0.36, 0.52, 0.82);
    if (d.y < 0.0) return lerp(hor, float3(0.42, 0.44, 0.47), saturate(-d.y * 2.0)) * 0.8;
    return lerp(hor, zen, sqrt(t)) * 0.8;
}

// ---------- ambiente HDRI: amostragem por importancia ----------
// Adaptado do InfiniteLight / Distribution2D do LuxCoreRender (Apache 2.0, (c) 1998-2020 LuxCoreRender authors),
// com o mapeamento latitude-longitude em Y para cima.
static const float INV_2PI = 0.15915494309;
static const float INV_PI = 0.31830988618;

float2 envDirToUV(float3 d, out float mapPdf)
{
    float theta = acos(clamp(d.y, -1.0, 1.0));
    float phi = atan2(d.z, d.x);
    if (phi < 0.0) phi += 2.0 * PI;
    float sinT = sin(theta);
    mapPdf = sinT > 1e-6 ? INV_2PI * INV_PI / sinT : 0.0;
    return float2(phi * INV_2PI, theta * INV_PI);
}

float3 envUVToDir(float2 uv, out float mapPdf)
{
    float phi = uv.x * 2.0 * PI;
    float theta = uv.y * PI;
    float sinT = sin(theta);
    mapPdf = sinT > 1e-6 ? INV_2PI * INV_PI / sinT : 0.0;
    return float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
}

// primeiro indice em [0, n) com val < cdf[i] (std::upper_bound)
uint envUpper(uint cdfBase, uint n, float val)
{
    uint first = 0u;
    uint count = n;
    while (count > 0u) {
        uint step = count / 2u;
        uint it = first + step;
        if (!(val < bEnvD[cdfBase + it])) {
            first = it + 1u;
            count -= step + 1u;
        } else {
            count = step;
        }
    }
    return first;
}

float envSample1D(uint base, float u, out float pdf, out uint off)
{
    uint count = asuint(bEnvD[base]);
    uint fB = base + 1u;
    uint cB = base + 1u + count;
    if (u <= bEnvD[cB]) {
        pdf = bEnvD[fB];
        off = 0u;
        return 0.0;
    }
    if (u >= bEnvD[cB + count]) {
        pdf = bEnvD[fB + count - 1u];
        off = count - 1u;
        return 1.0;
    }
    uint o = envUpper(cB, count + 1u, u) - 1u;
    float du = (u - bEnvD[cB + o]) / (bEnvD[cB + o + 1u] - bEnvD[cB + o]);
    pdf = bEnvD[fB + o];
    off = o;
    return (float(o) + du) / float(count);
}

float envPdf1D(uint base, float u)
{
    uint count = asuint(bEnvD[base]);
    uint o = min(count - 1u, uint(clamp(u, 0.0, 1.0) * float(count)));
    return bEnvD[base + 1u + o];
}

// amostra (u, v) do HDRI proporcional a luminancia; pdf em relacao a area de (u, v)
float2 envSample(float u0, float u1, out float pdf)
{
    uint w = asuint(bEnvD[0]);
    uint h = asuint(bEnvD[1]);
    uint mSize = 2u * h + 2u;
    uint cSize = 2u * w + 2u;
    float pdf1;
    uint row;
    float v = envSample1D(2u, u1, pdf1, row);
    float pdf0;
    uint dummy;
    float u = envSample1D(2u + mSize + row * cSize, u0, pdf0, dummy);
    pdf = pdf0 * pdf1;
    return float2(u, v);
}

float envPdf(float2 uv)
{
    uint w = asuint(bEnvD[0]);
    uint h = asuint(bEnvD[1]);
    uint mSize = 2u * h + 2u;
    uint cSize = 2u * w + 2u;
    uint row = min(h - 1u, uint(clamp(uv.y, 0.0, 1.0) * float(h)));
    return envPdf1D(2u + mSize + row * cSize, uv.x) * envPdf1D(2u, uv.y);
}

// radiancia do HDRI com filtro bilinear (volta em u, limita em v)
float3 envLookup(float2 uv)
{
    int w = int(asuint(bEnvD[0]));
    int h = int(asuint(bEnvD[1]));
    float fx = uv.x * float(w) - 0.5;
    float fy = uv.y * float(h) - 0.5;
    int x0 = int(floor(fx));
    int y0 = int(floor(fy));
    float tx = fx - float(x0);
    float ty = fy - float(y0);
    int xa = ((x0 % w) + w) % w;
    int xb = (((x0 + 1) % w) + w) % w;
    int ya = clamp(y0, 0, h - 1);
    int yb = clamp(y0 + 1, 0, h - 1);
    float3 c00 = bEnv[uint(ya * w + xa)].xyz;
    float3 c10 = bEnv[uint(ya * w + xb)].xyz;
    float3 c01 = bEnv[uint(yb * w + xa)].xyz;
    float3 c11 = bEnv[uint(yb * w + xb)].xyz;
    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

// visibilidade para luz direta: com vidro na cena, a sombra atravessa a transmissao (atenuada pela cor) em vez de bloquear
float3 shadowTrans(float3 o, float3 d)
{
    float3 T = float3(1.0);
    float3 org = o;
    for (int i = 0; i < 6; i++) {
        float tm = 1e30;
        int hit;
        float hu, hv;
        if (!traverse(org, d, tm, hit, hu, hv, false)) return T;
        float4 s0 = bShade[hit * 4];
        float4 s2 = bShade[hit * 4 + 2];
        float4 s3 = bShade[hit * 4 + 3];
        float tr = saturate(s0.w) * (1.0 - saturate(s2.w));
        if (tr < 0.5) return float3(0.0);
        T *= saturate(s3.xyz) * 0.9;
        org = org + d * (tm + 1e-4 * (1.0 + tm));
    }
    return T;
}

float3 visibility(float3 o, float3 d)
{
    if (pc.glass != 0) return shadowTrans(o, d);
    float tm = 1e30;
    int hit;
    float hu, hv;
    return traverse(o, d, tm, hit, hu, hv, true) ? float3(0.0) : float3(1.0);
}

void basis(float3 n, out float3 t, out float3 b)
{
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float bb = n.x * n.y * a;
    t = float3(1.0 + s * n.x * n.x * a, s * bb, -s * n.x);
    b = float3(bb, s + n.y * n.y * a, -n.y);
}

struct Mat {
    float3 cd;
    float3 f0;
    float a;
    float pSpec;     // probabilidade de escolher cada lobo na amostragem (o que sobra e o difuso)
    float pCoat;
    float pSheen;
    float coat;      // intensidade do verniz (clearcoat)
    float3 shC;      // cor do sheen, ja multiplicada pela intensidade
};

// Verniz (clearcoat) e sheen: BRDF do Disney, como no appleseed (MIT, (c) 2014-2018 Esteban Tovagliari, The appleseedhq Organization):
// verniz = GTR1 com Fresnel 0,04..1 e fator 0,25; sheen = (1 - cos)^5 com amostragem uniforme do hemisferio.
static const float COAT_ALPHA = 0.04;   // rugosidade fixa do verniz

float Dggx(float nh, float a2)
{
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float G1(float nv, float a2)
{
    return 2.0 * nv / (nv + sqrt(a2 + (1.0 - a2) * nv * nv));
}

// GTR1: distribuicao e termo de sombreamento de Smith (appleseed: GTR1MDF::D e GTR1MDF::lambda)
float Dgtr1(float nh, float a2)
{
    return (a2 - 1.0) / (PI * log(a2) * (1.0 + (a2 - 1.0) * nh * nh));
}
float lambdaGTR1(float cosT, float alpha)
{
    float c2 = cosT * cosT;
    float s2 = 1.0 - c2;
    if (cosT <= 0.0 || s2 <= 1e-12) return 0.0;
    float cot2 = c2 / s2;
    float cot = sqrt(cot2);
    float a2 = alpha * alpha;
    float a = sqrt(cot2 + a2);
    float b = sqrt(cot2 + 1.0);
    float c = log(cot + b);
    float d = log(cot + a);
    return (a - b + cot * (c - d)) / (cot * log(a2));
}

// wo e wi no espaco local (z = normal)
float3 evalBsdf(Mat m, float3 wo, float3 wi, out float pdf)
{
    pdf = 0.0;
    if (wi.z <= 0.0 || wo.z <= 0.0) return float3(0.0);
    float3 h = normalize(wo + wi);
    float nh = h.z;
    float vh = saturate(dot(wo, h));
    float nl = wi.z;
    float nv = wo.z;
    float a2 = m.a * m.a;
    float D = Dggx(nh, a2);
    float g1v = G1(nv, a2);
    float g1l = G1(nl, a2);
    float fm = pow(1.0 - vh, 5.0);
    float3 F = m.f0 + (1.0 - m.f0) * fm;
    float3 spec = F * (D * g1v * g1l / (4.0 * nv * nl));
    float3 diff = m.cd * (1.0 / PI);
    float pDiff = max(1.0 - m.pSpec - m.pCoat - m.pSheen, 0.0);
    pdf = pDiff * nl / PI + m.pSpec * D * g1v / (4.0 * nv);
    float3 f = diff + spec;
    if (m.pCoat > 0.0) {
        float ca2 = COAT_ALPHA * COAT_ALPHA;
        float Dc = Dgtr1(nh, ca2);
        float Gc = 1.0 / (1.0 + lambdaGTR1(nv, COAT_ALPHA) + lambdaGTR1(nl, COAT_ALPHA));
        float Fc = lerp(0.04, 1.0, fm) * 0.25 * m.coat;
        f += Fc * (Dc * Gc / (4.0 * nv * nl));
        pdf += m.pCoat * Dc * nh / (4.0 * max(vh, 1e-4));
    }
    if (m.pSheen > 0.0) {
        f += m.shC * pow(1.0 - saturate(dot(wi, h)), 5.0);
        pdf += m.pSheen / (2.0 * PI);
    }
    return f;
}

float3 sampleVNDF(float3 Ve, float a, float u1, float u2)
{
    float3 Vh = normalize(float3(a * Ve.x, a * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0.0 ? float3(-Vh.y, Vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    float3 T2 = cross(Vh, T1);
    float r = sqrt(u1);
    float phi = 2.0 * PI * u2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(max(0.0, 1.0 - t1 * t1)) + s * t2;
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    return normalize(float3(a * Nh.x, a * Nh.y, max(0.0, Nh.z)));
}

bool sampleBsdf(Mat m, float3 wo, float u0, float u1, float u2, out float3 wi)
{
    if (u0 < m.pSpec) {
        float3 h = sampleVNDF(wo, m.a, u1, u2);
        wi = 2.0 * dot(wo, h) * h - wo;
    } else if (u0 < m.pSpec + m.pCoat) {
        // normal de microfaceta do verniz: GTR1 (appleseed: GTR1MDF::sample)
        float a2 = COAT_ALPHA * COAT_ALPHA;
        float c2 = (1.0 - pow(a2, 1.0 - u1)) / (1.0 - a2);
        float ct = sqrt(max(c2, 0.0));
        float st = sqrt(max(1.0 - c2, 0.0));
        float phi = 2.0 * PI * u2;
        float3 h = float3(st * cos(phi), st * sin(phi), ct);
        wi = 2.0 * dot(wo, h) * h - wo;
    } else if (u0 < m.pSpec + m.pCoat + m.pSheen) {
        float r = sqrt(max(1.0 - u1 * u1, 0.0));    // hemisferio uniforme (z = u1)
        float phi = 2.0 * PI * u2;
        wi = float3(r * cos(phi), r * sin(phi), u1);
    } else {
        float r = sqrt(u1);
        float phi = 2.0 * PI * u2;
        wi = float3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u1)));
    }
    return wi.z > 0.0;
}

[numthreads(8, 8, 1)]
void csTrace(uint3 id : SV_DispatchThreadID)
{
    int px = int(id.x);
    int py = pc.y0 + int(id.y);
    if (px >= pc.w || int(id.y) >= pc.rows || py >= pc.h) return;
    uint st = pcg(uint(px) + pcg(uint(py) + pcg(uint(pc.sample) + 0x9e3779b9u)));
    float jx = rnd(st);
    float jy = rnd(st);
    float nx = (float(px) + jx) / float(pc.w) * 2.0 - 1.0;
    float ny = 1.0 - (float(py) + jy) / float(pc.h) * 2.0;
    float3 d = normalize(pc.fwd + pc.right * (nx * pc.tanY * pc.aspect) + pc.up * (ny * pc.tanY));
    float3 o = pc.eye;
    float3 L = float3(0.0);
    float3 beta = float3(1.0);
    float3 sunD = sunDir();
    float3 sunE = sunIrr(pc.night);
    float3 aovN = float3(0.0);   // normal, profundidade e albedo do primeiro hit (para o denoise)
    float aovD = 1e4;
    float3 aovA = float3(1.0);
    bool inMed = false;          // dentro de um meio com absorcao (vidro, agua)
    float3 sigA = float3(0.0);
    bool prevDelta = false;      // o ultimo rebatimento foi especular (vidro): sem MIS com o ambiente
    int glassN = 0;              // interacoes com vidro nao contam como rebatimentos
    float pathLen = 0.0;         // distancia percorrida desde a camera (profundidade do AOV atraves do vidro)
    bool aovDone = false;        // o AOV vem da primeira superficie que nao e vidro
    float prevPdf = 1.0;         // pdf (angulo solido) da direcao amostrada no rebatimento anterior, para o MIS com o ambiente
    // cone do sol (abertura angular ~1,7 grau: sombras suaves)
    const float cosMax = 0.99955;
    const float cosSpec = 0.9962;   // cone maior (~5 graus) para o sol visto por reflexo/refracao: menos pontos brancos

    for (int bounce = 0; bounce < pc.bounces + 12; bounce++) {
        if (bounce - glassN >= pc.bounces) break;
        float tmax = 1e30;
        int ti;
        float hu, hv;
        if (!traverse(o, d, tmax, ti, hu, hv, false)) {
            if (pc.envOn != 0) {
                float mp;
                float2 euv = envDirToUV(d, mp);
                float3 er = envLookup(euv) * pc.envScale;
                float wB = 1.0;
                if (bounce > 0 && mp > 0.0 && !prevDelta) {
                    float pl = envPdf(euv) * mp;
                    wB = (prevPdf * prevPdf) / (prevPdf * prevPdf + pl * pl);
                }
                L += beta * er * wB;
            } else {
                L += beta * skyRad(d, pc.night);
                // o sol visto por reflexo ou refracao (vidro): disco do cone
                if (prevDelta && dot(d, sunD) > cosSpec) L += beta * sunE / (2.0 * PI * (1.0 - cosSpec));
            }
            break;
        }
        if (inMed) beta *= exp(-sigA * tmax);      // absorcao (Beer-Lambert) no trecho percorrido dentro do vidro
        pathLen += tmax;
        float w0 = 1.0 - hu - hv;
        float4 s0 = bShade[ti * 4];
        float4 s1 = bShade[ti * 4 + 1];
        float4 s2 = bShade[ti * 4 + 2];
        float4 s3 = bShade[ti * 4 + 3];
        float4 tA = bTexI[ti * 3];
        float4 tB = bTexI[ti * 3 + 1];
        float4 tC = bTexI[ti * 3 + 2];
        float3 p = o + d * tmax;
        float3 e1 = bPos[ti * 3 + 1].xyz - bPos[ti * 3].xyz;
        float3 e2 = bPos[ti * 3 + 2].xyz - bPos[ti * 3].xyz;
        float3 ng = normalize(cross(e1, e2));
        float3 ns = w0 * s0.xyz + hu * s1.xyz + hv * s2.xyz;
        float nl2 = dot(ns, ns);
        ns = nl2 > 1e-12 ? ns * rsqrt(nl2) : ng;
        float3 nOut = ns;

        // texturas de material
        float2 uv = w0 * tA.xy + hu * tA.zw + hv * tB.xy;
        int lb = int(tB.z);
        int lm = int(tB.w);
        int ln = int(tC.x);
        float metal = saturate(s2.w);
        float rou = s3.w;
        float3 col = saturate(s3.xyz);
        if (lb >= 0 && lb < pc.ntex) col *= pow(saturate(uTex.SampleLevel(float3(uv, float(lb)), 0.0).rgb), float3(2.2));
        if (lm >= 0 && lm < pc.ntex) {
            float4 mrs = uTex.SampleLevel(float3(uv, float(lm)), 0.0);
            rou *= mrs.g;
            metal = saturate(metal * mrs.b);
        }
        if (ln >= 0 && ln < pc.ntex) {
            // mapa de normais: tangente a partir das coordenadas de textura do triangulo
            float3 tn = uTex.SampleLevel(float3(uv, float(ln)), 0.0).xyz * 2.0 - 1.0;
            float2 du1 = tA.zw - tA.xy;
            float2 du2 = tB.xy - tA.xy;
            float det = du1.x * du2.y - du1.y * du2.x;
            if (abs(det) > 1e-12) {
                float3 Tt = (e1 * du2.y - e2 * du1.y) / det;
                float3 Tu = Tt - ns * dot(ns, Tt);
                float tl2 = dot(Tu, Tu);
                if (tl2 > 1e-14) {
                    Tu *= rsqrt(tl2);
                    float3 Bu = cross(ns, Tu);
                    float3 Bt = -((e2 * du1.x - e1 * du2.x) / det);   // mesmo sentido do viewport (v para baixo)
                    if (dot(Bu, Bt) < 0.0) Bu = -Bu;
                    ns = normalize(Tu * tn.x + Bu * tn.y + ns * tn.z);
                }
            }
        }
        bool backf = dot(nOut, d) > 0.0;     // o raio esta saindo do objeto (vem por dentro)
        if (dot(ns, d) > 0.0) ns = -ns;      // sempre do lado de quem olha
        if (dot(ng, ns) < 0.0) ng = -ng;
        rou = clamp(rou, 0.04, 1.0);
        Mat m;
        m.cd = col * (1.0 - metal);
        m.f0 = lerp(float3(0.04), col, metal);
        m.a = rou * rou;
        float ls = dot(m.f0, float3(0.3, 0.6, 0.1));
        float ld_ = dot(m.cd, float3(0.3, 0.6, 0.1));
        float pBase = clamp(ls / max(ls + ld_, 1e-6), 0.1, 1.0);
        float coatA = saturate(tC.z);                  // verniz
        float sheenA = saturate(tC.w) * (1.0 - metal); // sheen (metal nao tem)
        float wC = 0.25 * coatA;
        float wH = 0.5 * sheenA;
        float wTot = 1.0 + wC + wH;                    // pesos: difuso (1 - pBase) + especular (pBase) + verniz + sheen
        m.pSpec = pBase / wTot;
        m.pCoat = wC / wTot;
        m.pSheen = wH / wTot;
        m.coat = coatA;
        float lumc = dot(col, float3(0.3, 0.6, 0.1));
        float3 tint = lumc > 1e-4 ? col / lumc : float3(1.0);
        m.shC = lerp(float3(1.0), tint, 0.5) * sheenA;
        float trn = saturate(s0.w) * (1.0 - metal);   // chance de o raio interagir como vidro
        if (!aovDone && trn < 0.5) {   // o denoise usa a primeira superficie que nao e vidro (o que se ve atraves dele)
            aovDone = true;
            aovN = ns;
            aovD = pathLen;
            aovA = max(m.cd + m.f0, float3(0.02));
        }

        float3 T, B;
        basis(ns, T, B);
        float3 wo = float3(dot(-d, T), dot(-d, B), dot(-d, ns));
        float eps = 1e-4 * (1.0 + max(abs(p.x), max(abs(p.y), abs(p.z))));
        float3 org = p + ng * eps;

        // vidro: reflete ou refrata pelo Fresnel (dieletrico) e o raio segue; sem luz direta (a luz vem pelos caminhos)
        if (trn > 0.0 && rnd(st) < trn) {
            float ior = max(s1.w, 1.0);
            float rr = backf ? ior : 1.0 / ior;          // n do meio de origem / n do meio de destino
            float cosI = saturate(dot(-d, ns));
            float kk = 1.0 - rr * rr * (1.0 - cosI * cosI);
            float F = 1.0;
            float cosT = 0.0;
            if (kk > 0.0) {
                cosT = sqrt(kk);
                float rs = (rr * cosI - cosT) / (rr * cosI + cosT);
                float rp = (cosI - rr * cosT) / (cosI + rr * cosT);
                F = 0.5 * (rs * rs + rp * rp);
            }
            if (rnd(st) < F) {
                d = reflect(d, ns);
                o = org;
            } else {
                d = normalize(rr * d + (rr * cosI - cosT) * ns);
                o = p - ng * eps;
                if (backf) {
                    inMed = false;
                } else {
                    inMed = true;
                    sigA = -log(max(col, float3(0.002)));
                }
            }
            prevDelta = true;
            glassN++;
            continue;
        }
        prevDelta = false;

        // luz direta: sol (cone) com raio de sombra, ou o HDRI amostrado por importancia (com MIS)
        if (pc.envOn == 0) {
            float u1 = rnd(st);
            float u2 = rnd(st);
            float cz = 1.0 - u1 * (1.0 - cosMax);
            float sz = sqrt(max(0.0, 1.0 - cz * cz));
            float ph = 2.0 * PI * u2;
            float3 st1, st2;
            basis(sunD, st1, st2);
            float3 ldir = normalize(st1 * (sz * cos(ph)) + st2 * (sz * sin(ph)) + sunD * cz);
            float3 wl = float3(dot(ldir, T), dot(ldir, B), dot(ldir, ns));
            if (wl.z > 0.0 && dot(ldir, ng) > 0.0) {
                float3 vis = visibility(org, ldir);
                if (any(vis > 0.0)) {
                    float pdf0;
                    float3 f = evalBsdf(m, wo, wl, pdf0);
                    L += beta * f * (wl.z) * sunE * vis;
                }
            }
        } else {
            float eu0 = rnd(st);
            float eu1 = rnd(st);
            float pdfL;
            float2 suv = envSample(eu0, eu1, pdfL);
            float mp2;
            float3 ldir = envUVToDir(suv, mp2);
            pdfL *= mp2;
            float3 wl = float3(dot(ldir, T), dot(ldir, B), dot(ldir, ns));
            if (pdfL > 1e-9 && wl.z > 0.0 && dot(ldir, ng) > 0.0) {
                float3 vis = visibility(org, ldir);
                if (any(vis > 0.0)) {
                    float pdfB;
                    float3 f = evalBsdf(m, wo, wl, pdfB);
                    float wL = (pdfL * pdfL) / (pdfL * pdfL + pdfB * pdfB);
                    L += beta * f * (wl.z / pdfL) * (envLookup(suv) * pc.envScale) * wL * vis;
                }
            }
        }

        // proxima direcao
        float u0 = rnd(st);
        float v1 = rnd(st);
        float v2 = rnd(st);
        float3 wi;
        if (!sampleBsdf(m, wo, u0, v1, v2, wi)) break;
        float pdf;
        float3 f = evalBsdf(m, wo, wi, pdf);
        if (pdf < 1e-7) break;
        prevPdf = pdf;
        beta *= f * (wi.z / pdf);
        if (bounce >= 3) {
            float q = clamp(max(beta.x, max(beta.y, beta.z)), 0.05, 0.95);
            if (rnd(st) > q) break;
            beta /= q;
        }
        d = normalize(wi.x * T + wi.y * B + wi.z * ns);
        o = org;
    }
    if (any(isnan(L)) || any(isinf(L))) L = float3(0.0);
    float lmax = pc.envOn != 0 ? 100.0 : 30.0;
    if (glassN > 0) lmax = min(lmax, 16.0);   // caminhos com vidro tem muito brilho isolado (caustica)
    L = min(L, float3(lmax));
    uint idx = uint(py) * uint(pc.w) + uint(px);
    if (pc.dn != 0) {
        uint bi = idx * 3u;
        float4 p0 = pc.sample == 0 ? float4(0.0) : bAcc[bi];
        float4 p1 = pc.sample == 0 ? float4(0.0) : bAcc[bi + 1u];
        float4 p2 = pc.sample == 0 ? float4(0.0) : bAcc[bi + 2u];
        bAcc[bi] = p0 + float4(L, 1.0);
        bAcc[bi + 1u] = p1 + float4(aovN, aovD);
        bAcc[bi + 2u] = p2 + float4(aovA, 1.0);
    } else {
        float4 prev = pc.sample == 0 ? float4(0.0) : bAcc[idx];
        bAcc[idx] = prev + float4(L, 1.0);
    }
}

float3 shoulder(float3 c)
{
    // linear ate 0,55; acima disso comprime suavemente, para o claro nao estourar e chapar a cor
    float3 hi = 0.55 + 0.45 * tanh((c - 0.55) / 0.45);
    return float3(c.x < 0.55 ? c.x : hi.x, c.y < 0.55 ? c.y : hi.y, c.z < 0.55 ? c.z : hi.z);
}
float3 toSrgb(float3 c)
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, float3(0.0031308)), float3(1.0 / 2.4)) - 0.055;
    return float3(c.x <= 0.0031308 ? lo.x : hi.x, c.y <= 0.0031308 ? lo.y : hi.y, c.z <= 0.0031308 ? lo.z : hi.z);
}

// ACES (aproximacao de Narkowicz, 2015)
float3 acesFit(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

[numthreads(8, 8, 1)]
void csResolve(uint3 id : SV_DispatchThreadID)
{
    int px = int(id.x);
    int py = int(id.y);
    if (px >= pc.w || py >= pc.h) return;
    uint idx = uint(py) * uint(pc.w) + uint(px);
    float3 c;
    if (pc.dn != 0) {
        float cnt = max(bAcc[idx * 3u].w, 1.0);
        float3 alb = bAcc[idx * 3u + 2u].xyz / cnt;
        c = bTmp[idx * 2u + uint(pc.src)].xyz * max(alb, float3(0.02));
    } else {
        float4 a = bAcc[idx];
        c = a.xyz / max(a.w, 1.0);
    }
    c = max(c, float3(0.0)) * pc.expo;
    if (pc.tonemap == 1) c = acesFit(c);
    else c = shoulder(c);
    c = saturate(toSrgb(saturate(c)));
    uint r = uint(c.x * 255.0 + 0.5);
    uint g = uint(c.y * 255.0 + 0.5);
    uint b = uint(c.z * 255.0 + 0.5);
    bOut[idx] = r | (g << 8u) | (b << 16u) | (255u << 24u);
}

// denoise, passo 1: iluminacao desmodulada pelo albedo (a textura nao e borrada junto)
[numthreads(8, 8, 1)]
void csDnPrep(uint3 id : SV_DispatchThreadID)
{
    int px = int(id.x);
    int py = pc.y0 + int(id.y);
    if (px >= pc.w || int(id.y) >= pc.rows || py >= pc.h) return;
    uint idx = uint(py) * uint(pc.w) + uint(px);
    float4 a = bAcc[idx * 3u];
    float cnt = max(a.w, 1.0);
    float3 alb = bAcc[idx * 3u + 2u].xyz / cnt;
    bTmp[idx * 2u] = float4(a.xyz / cnt / max(alb, float3(0.02)), 0.0);
}

// denoise, passos seguintes: a-trous 5x5 guiado por normal, profundidade e luminancia
[numthreads(8, 8, 1)]
void csDnStep(uint3 id : SV_DispatchThreadID)
{
    int px = int(id.x);
    int py = pc.y0 + int(id.y);
    if (px >= pc.w || int(id.y) >= pc.rows || py >= pc.h) return;
    uint idx = uint(py) * uint(pc.w) + uint(px);
    uint dst = idx * 2u + uint(1 - pc.src);
    float3 c0 = bTmp[idx * 2u + uint(pc.src)].xyz;
    float4 na = bAcc[idx * 3u + 1u];
    float cn0 = max(bAcc[idx * 3u].w, 1.0);
    float3 n0 = na.xyz / cn0;
    float d0 = na.w / cn0;
    if (dot(n0, n0) < 0.25) {      // ceu: sem geometria para guiar, passa direto
        bTmp[dst] = float4(c0, 0.0);
        return;
    }
    n0 = normalize(n0);
    float l0 = dot(c0, float3(0.299, 0.587, 0.114));
    float kern[3] = { 0.375, 0.25, 0.0625 };
    float3 sum = c0 * (kern[0] * kern[0]);
    float ws = kern[0] * kern[0];
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx == 0 && dy == 0) continue;
            int qx = px + dx * pc.stp;
            int qy = py + dy * pc.stp;
            if (qx < 0 || qy < 0 || qx >= pc.w || qy >= pc.h) continue;
            uint qi = uint(qy) * uint(pc.w) + uint(qx);
            float4 qa = bAcc[qi * 3u + 1u];
            float qc = max(bAcc[qi * 3u].w, 1.0);
            float3 qn = qa.xyz / qc;
            if (dot(qn, qn) < 0.25) continue;
            qn = normalize(qn);
            float qd = qa.w / qc;
            float3 c1 = bTmp[qi * 2u + uint(pc.src)].xyz;
            float wn = pow(saturate(dot(n0, qn)), 48.0);
            float wd = exp(-abs(d0 - qd) / (0.06 * max(d0, 0.05) + 1e-3));
            float l1 = dot(c1, float3(0.299, 0.587, 0.114));
            float wl = exp(-abs(l0 - l1) / max(pc.phi * 0.25 * (l0 + l1 + 0.1), 1e-3));
            float w = kern[abs(dx)] * kern[abs(dy)] * wn * wd * wl;
            sum += c1 * w;
            ws += w;
        }
    }
    bTmp[dst] = float4(sum / max(ws, 1e-6), 0.0);
}
