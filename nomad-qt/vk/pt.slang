// Parallax: path tracer em compute (nucleo da Etapa 3)
// cena: triangulos + BVH binaria (construida na CPU); sol com sombra suave + ceu; material difuso + GGX (metal/rugosidade)
struct PC {
    float3 eye;
    float tanY;
    float3 right;
    float aspect;
    float3 up;
    float pad0;
    float3 fwd;
    float pad1;
    int w;
    int h;
    int y0;
    int rows;
    int sample;
    int bounces;
    int night;
    int ntri;
};
[[vk::push_constant]] ConstantBuffer<PC> pc;

[[vk::binding(0, 0)]] StructuredBuffer<float4> bPos;    // 3 float4 por triangulo (p0, p1, p2)
[[vk::binding(1, 0)]] StructuredBuffer<float4> bShade;  // 4 float4: n0, n1, n2 (w de n2 = metal), cor (w = rugosidade)
[[vk::binding(2, 0)]] StructuredBuffer<float4> bNode;   // 2 float4 por no: (min, esq/primeiro), (max, quantidade)
[[vk::binding(3, 0)]] RWStructuredBuffer<float4> bAcc;  // soma das amostras (rgb) e contagem (a)
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> bOut;    // RGBA8 final, de cima para baixo

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

// percorre a BVH; anyHit = so quer saber se algo bloqueia (sombra)
bool traverse(float3 o, float3 d, inout float tmax, out int hit, out float hu, out float hv, bool anyHit)
{
    float3 inv = float3(sinv(d.x), sinv(d.y), sinv(d.z));
    int stack[32];
    int sp = 0;
    int node = 0;
    hit = -1;
    hu = 0.0;
    hv = 0.0;
    while (true) {
        float4 a = bNode[node * 2];
        float4 b = bNode[node * 2 + 1];
        float3 t0 = (a.xyz - o) * inv;
        float3 t1 = (b.xyz - o) * inv;
        float3 tmn = min(t0, t1);
        float3 tmx = max(t0, t1);
        float tn = max(max(tmn.x, tmn.y), max(tmn.z, 0.0));
        float tf = min(min(tmx.x, tmx.y), min(tmx.z, tmax));
        bool inside = tn <= tf;
        int cnt = asint(b.w);
        if (inside) {
            if (cnt > 0) {
                int first = asint(a.w);
                for (int i = 0; i < cnt; i++) {
                    float u, v;
                    if (hitTri(o, d, first + i, tmax, u, v)) {
                        hit = first + i;
                        hu = u;
                        hv = v;
                        if (anyHit) return true;
                    }
                }
            } else {
                if (sp < 31) stack[sp++] = asint(a.w);
                node = node + 1;
                continue;
            }
        }
        if (sp == 0) break;
        node = stack[--sp];
    }
    return hit >= 0;
}

float3 sunDir()
{
    return normalize(float3(0.45, 0.75, 0.35));
}
float3 sunIrr(int night)
{
    return night != 0 ? float3(0.10, 0.13, 0.22) : float3(2.6, 2.45, 2.2);
}
float3 skyRad(float3 d, int night)
{
    float t = saturate(d.y);
    if (night != 0) {
        float3 s = lerp(float3(0.020, 0.028, 0.050), float3(0.004, 0.007, 0.020), sqrt(t));
        return d.y < 0.0 ? s * 0.5 : s;
    }
    float3 hor = float3(0.85, 0.90, 1.00);
    float3 zen = float3(0.28, 0.50, 0.95);
    if (d.y < 0.0) return lerp(hor, float3(0.30, 0.28, 0.26), saturate(-d.y * 4.0));
    return lerp(hor, zen, sqrt(t));
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
    float pSpec;
};

float Dggx(float nh, float a2)
{
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float G1(float nv, float a2)
{
    return 2.0 * nv / (nv + sqrt(a2 + (1.0 - a2) * nv * nv));
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
    pdf = (1.0 - m.pSpec) * nl / PI + m.pSpec * D * g1v / (4.0 * nv);
    return diff + spec;
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
    // cone do sol (abertura angular ~1,7 grau: sombras suaves)
    const float cosMax = 0.99955;

    for (int bounce = 0; bounce < pc.bounces; bounce++) {
        float tmax = 1e30;
        int ti;
        float hu, hv;
        if (!traverse(o, d, tmax, ti, hu, hv, false)) {
            L += beta * skyRad(d, pc.night);
            break;
        }
        float w0 = 1.0 - hu - hv;
        float4 s0 = bShade[ti * 4];
        float4 s1 = bShade[ti * 4 + 1];
        float4 s2 = bShade[ti * 4 + 2];
        float4 s3 = bShade[ti * 4 + 3];
        float3 p = o + d * tmax;
        float3 e1 = bPos[ti * 3 + 1].xyz - bPos[ti * 3].xyz;
        float3 e2 = bPos[ti * 3 + 2].xyz - bPos[ti * 3].xyz;
        float3 ng = normalize(cross(e1, e2));
        float3 ns = w0 * s0.xyz + hu * s1.xyz + hv * s2.xyz;
        float nl2 = dot(ns, ns);
        ns = nl2 > 1e-12 ? ns * rsqrt(nl2) : ng;
        if (dot(ns, d) > 0.0) ns = -ns;      // sempre do lado de quem olha
        if (dot(ng, ns) < 0.0) ng = -ng;
        float metal = saturate(s2.w);
        float rou = clamp(s3.w, 0.04, 1.0);
        float3 col = saturate(s3.xyz);
        Mat m;
        m.cd = col * (1.0 - metal);
        m.f0 = lerp(float3(0.04), col, metal);
        m.a = rou * rou;
        float ls = dot(m.f0, float3(0.3, 0.6, 0.1));
        float ld_ = dot(m.cd, float3(0.3, 0.6, 0.1));
        m.pSpec = clamp(ls / max(ls + ld_, 1e-6), 0.1, 1.0);

        float3 T, B;
        basis(ns, T, B);
        float3 wo = float3(dot(-d, T), dot(-d, B), dot(-d, ns));
        float eps = 1e-4 * (1.0 + max(abs(p.x), max(abs(p.y), abs(p.z))));
        float3 org = p + ng * eps;

        // luz direta: sol (cone) com raio de sombra
        {
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
                float tm2 = 1e30;
                int sh;
                float su, sv;
                if (!traverse(org, ldir, tm2, sh, su, sv, true)) {
                    float pdf0;
                    float3 f = evalBsdf(m, wo, wl, pdf0);
                    L += beta * f * (wl.z) * sunE;
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
    L = min(L, float3(30.0));
    uint idx = uint(py) * uint(pc.w) + uint(px);
    float4 prev = pc.sample == 0 ? float4(0.0) : bAcc[idx];
    bAcc[idx] = prev + float4(L, 1.0);
}

float3 shoulder(float3 c)
{
    // linear ate 0,8; acima disso comprime suavemente (sem mudar os tons medios)
    float3 hi = 0.8 + 0.2 * tanh((c - 0.8) / 0.2);
    return float3(c.x < 0.8 ? c.x : hi.x, c.y < 0.8 ? c.y : hi.y, c.z < 0.8 ? c.z : hi.z);
}
float3 toSrgb(float3 c)
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, float3(0.0031308)), float3(1.0 / 2.4)) - 0.055;
    return float3(c.x <= 0.0031308 ? lo.x : hi.x, c.y <= 0.0031308 ? lo.y : hi.y, c.z <= 0.0031308 ? lo.z : hi.z);
}

[numthreads(8, 8, 1)]
void csResolve(uint3 id : SV_DispatchThreadID)
{
    int px = int(id.x);
    int py = int(id.y);
    if (px >= pc.w || py >= pc.h) return;
    uint idx = uint(py) * uint(pc.w) + uint(px);
    float4 a = bAcc[idx];
    float3 c = a.xyz / max(a.w, 1.0);
    c = saturate(toSrgb(saturate(shoulder(max(c, float3(0.0))))));
    uint r = uint(c.x * 255.0 + 0.5);
    uint g = uint(c.y * 255.0 + 0.5);
    uint b = uint(c.z * 255.0 + 0.5);
    bOut[idx] = r | (g << 8u) | (b << 16u) | (255u << 24u);
}
