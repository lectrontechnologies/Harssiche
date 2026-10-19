#include "meshes.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr float kPi = 3.14159265358979f;

struct V3 {
    float x, y, z;
};
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 norm(V3 a) {
    float l = std::sqrt(dot(a, a));
    return l > 1e-8f ? a * (1.f / l) : V3{0, 1, 0};
}

void pushV(MeshData &m, V3 p, V3 n) {
    m.v.insert(m.v.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}
void tri(MeshData &m, int a, int b, int c) {
    m.i.push_back(uint16_t(a));
    m.i.push_back(uint16_t(b));
    m.i.push_back(uint16_t(c));
}
int vcount(const MeshData &m) { return int(m.v.size() / 6); }

// Face quadrada: u x v = n, vertices (-,-) (+,-) (+,+) (-,+) => CCW vista de fora.
void addFace(MeshData &m, V3 n, V3 u, V3 v, V3 center, float half) {
    int base = vcount(m);
    pushV(m, center + u * (-half) + v * (-half), n);
    pushV(m, center + u * (half) + v * (-half), n);
    pushV(m, center + u * (half) + v * (half), n);
    pushV(m, center + u * (-half) + v * (half), n);
    tri(m, base, base + 1, base + 2);
    tri(m, base, base + 2, base + 3);
}

MeshData makeCube() {
    MeshData m;
    const V3 X{1, 0, 0}, Y{0, 1, 0}, Z{0, 0, 1};
    addFace(m, X, Y, Z, X * 0.5f, 0.5f);
    addFace(m, X * -1.f, Z, Y, X * -0.5f, 0.5f);
    addFace(m, Y, Z, X, Y * 0.5f, 0.5f);
    addFace(m, Y * -1.f, X, Z, Y * -0.5f, 0.5f);
    addFace(m, Z, X, Y, Z * 0.5f, 0.5f);
    addFace(m, Z * -1.f, Y, X, Z * -0.5f, 0.5f);
    return m;
}

MeshData makePlane() {
    MeshData m;
    addFace(m, V3{0, 1, 0}, V3{0, 0, 1}, V3{1, 0, 0}, V3{0, 0, 0}, 0.5f);
    return m;
}

MeshData makeSphere(int seg, int rings) {
    MeshData m;
    for (int r = 0; r <= rings; ++r) {
        float th = kPi * r / rings;
        for (int s = 0; s <= seg; ++s) {
            float ph = 2 * kPi * s / seg;
            V3 n{std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)};
            pushV(m, n * 0.5f, n);
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < seg; ++s) {
            int a = r * (seg + 1) + s, b = a + seg + 1;
            tri(m, a, a + 1, b);
            tri(m, a + 1, b + 1, b);
        }
    return m;
}

void addCap(MeshData &m, int seg, float r, float y, bool up) {
    V3 n{0, up ? 1.f : -1.f, 0};
    int c = vcount(m);
    pushV(m, V3{0, y, 0}, n);
    for (int s = 0; s <= seg; ++s) {
        float ph = 2 * kPi * s / seg;
        pushV(m, V3{r * std::cos(ph), y, r * std::sin(ph)}, n);
    }
    for (int s = 0; s < seg; ++s) {
        int a = c + 1 + s, b = c + 2 + s;
        if (up) tri(m, c, b, a);
        else tri(m, c, a, b);
    }
}

MeshData makeFrustum(int seg, float rTop, float rBot, bool capTop, bool capBot) {
    MeshData m;
    const float h = 1.f;
    for (int s = 0; s <= seg; ++s) {
        float ph = 2 * kPi * s / seg, c = std::cos(ph), sn = std::sin(ph);
        V3 n = norm(V3{c * h, rBot - rTop, sn * h});
        pushV(m, V3{rTop * c, 0.5f, rTop * sn}, n);
        pushV(m, V3{rBot * c, -0.5f, rBot * sn}, n);
    }
    for (int s = 0; s < seg; ++s) {
        int a = 2 * s, a1 = 2 * (s + 1), b = 2 * s + 1, b1 = 2 * (s + 1) + 1;
        tri(m, a, a1, b);
        tri(m, a1, b1, b);
    }
    if (capTop) addCap(m, seg, rTop, 0.5f, true);
    if (capBot) addCap(m, seg, rBot, -0.5f, false);
    return m;
}

MeshData makeTorus(int seg, int tube) {
    MeshData m;
    const float R = 0.35f, r = 0.15f;
    for (int i = 0; i <= seg; ++i) {
        float u = 2 * kPi * i / seg;
        for (int j = 0; j <= tube; ++j) {
            float v = 2 * kPi * j / tube;
            V3 n{std::cos(v) * std::cos(u), std::sin(v), std::cos(v) * std::sin(u)};
            V3 p{(R + r * std::cos(v)) * std::cos(u), r * std::sin(v), (R + r * std::cos(v)) * std::sin(u)};
            pushV(m, p, n);
        }
    }
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < tube; ++j) {
            int a = i * (tube + 1) + j, b = (i + 1) * (tube + 1) + j;
            tri(m, a, a + 1, b);
            tri(m, a + 1, b + 1, b);
        }
    return m;
}

float clamp01(float x) { return std::min(1.f, std::max(0.f, x)); }
}  // namespace

MeshData makeMesh(int type) {
    switch (type) {
    case PrimSphere: return makeSphere(32, 16);
    case PrimCylinder: return makeFrustum(32, 0.5f, 0.5f, true, true);
    case PrimCone: return makeFrustum(32, 0.f, 0.5f, false, true);
    case PrimTorus: return makeTorus(32, 16);
    case PrimPlane: return makePlane();
    default: return makeCube();
    }
}

void primHalfExtents(int type, float o[3]) {
    o[0] = o[1] = o[2] = 0.5f;
    if (type == PrimTorus) o[1] = 0.15f;
    if (type == PrimPlane) o[1] = 0.02f;
}

// Matcap procedural: azul suave, luz principal no alto a esquerda, preenchimento,
// brilho especular e leve luz de contorno. Pode ser trocado por uma imagem depois.
std::vector<uint8_t> makeMatcap(int N) {
    std::vector<uint8_t> px(size_t(N) * N * 4);
    const V3 L1 = norm({-0.45f, 0.65f, 0.62f});
    const V3 L2 = norm({0.70f, -0.20f, 0.35f});
    const V3 Vw{0, 0, 1};
    const V3 H = norm(L1 + Vw);
    const V3 base{0.42f, 0.47f, 0.86f};
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float fx = (x + 0.5f) / N * 2.f - 1.f, fy = (y + 0.5f) / N * 2.f - 1.f;
            float r2 = fx * fx + fy * fy;
            V3 n = r2 < 1.f ? V3{fx, fy, std::sqrt(1.f - r2)} : norm({fx, fy, 0.f});
            float d1 = std::max(0.f, dot(n, L1)), d2 = std::max(0.f, dot(n, L2));
            float hemi = 0.5f + 0.5f * n.y;
            float k = 0.30f + 0.80f * d1 + 0.22f * d2 + 0.25f * hemi;
            V3 c = base * k;
            float spec = std::pow(std::max(0.f, dot(n, H)), 48.f) * 0.55f;
            c = c + V3{0.90f, 0.93f, 1.0f} * spec;
            float rim = std::pow(clamp01(1.f - n.z), 3.f) * 0.18f;
            c = c + V3{0.55f, 0.65f, 1.0f} * rim;
            uint8_t *o = &px[(size_t(y) * N + x) * 4];
            o[0] = uint8_t(clamp01(c.x) * 255.f + 0.5f);
            o[1] = uint8_t(clamp01(c.y) * 255.f + 0.5f);
            o[2] = uint8_t(clamp01(c.z) * 255.f + 0.5f);
            o[3] = 255;
        }
    return px;
}
