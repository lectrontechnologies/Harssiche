#version 450
// Porta do shader do motor (sombreamento PBR simples, matcap, retangulos arredondados, losango e fundo).
// Ainda sem texturas de material (glTF): usa a cor base.
struct Op {
    mat4 mvp;
    mat4 mod;
    vec4 col;
    vec4 a;
    vec4 b;
    vec4 tl;
};
layout(std430, set = 0, binding = 0) readonly buffer Ops { Op ops[]; };
layout(push_constant) uniform PC {
    vec4 cam;   // xyz = posicao da camera, w = matcap ligado
    vec4 mcR;   // base do matcap; w = altura da tela
    vec4 mcU;
    vec4 mcB;
} pc;

layout(location = 0) in vec3 vN;
layout(location = 1) in vec3 vP;
layout(location = 2) in vec3 vW;
layout(location = 3) flat in int vOp;
layout(location = 0) out vec4 o;

vec3 shade(vec3 L, vec3 Lc, vec3 N, vec3 V, vec3 alb, vec3 F0, float met, float rou) {
    vec3 H = normalize(L + V);
    float a = rou * rou, a2 = a * a;
    float NL = max(dot(N, L), 0.0), NV = max(dot(N, V), 1e-3), NH = max(dot(N, H), 0.0), VH = max(dot(V, H), 0.0);
    float dd = NH * NH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * dd * dd + 1e-6);
    float k = (rou + 1.0) * (rou + 1.0) / 8.0;
    float G = (NL / (NL * (1.0 - k) + k)) * (NV / (NV * (1.0 - k) + k));
    vec3 F = F0 + (1.0 - F0) * pow(max(1.0 - VH, 0.0), 5.0);
    vec3 spec = D * G * F / (4.0 * NV * max(NL, 1e-3));
    vec3 kd = (1.0 - F) * (1.0 - met);
    return (kd * alb / 3.14159 + spec) * Lc * NL;
}

vec3 pbr(vec4 uC, vec2 uMR) {
    vec3 N = normalize(vN), V = normalize(pc.cam.xyz - vW);
    if (dot(N, V) < 0.0) N = -N;
    vec3 base = uC.rgb;
    float met = uMR.x, rou = uMR.y;
    vec3 alb = pow(base, vec3(2.2));
    rou = clamp(rou, 0.05, 1.0);
    vec3 F0 = mix(vec3(0.04), alb, met);
    vec3 col = shade(normalize(vec3(.4, .8, .5)), vec3(3.2, 3.0, 2.8), N, V, alb, F0, met, rou)
             + shade(normalize(vec3(-.6, .3, -.4)), vec3(.6, .7, 1.0), N, V, alb, F0, met, rou);
    vec3 amb = mix(vec3(.12, .11, .10), vec3(.22, .26, .34), N.y * .5 + .5);
    float NV = max(dot(N, V), 0.0);
    vec3 Fa = F0 + (max(vec3(1.0 - rou), F0) - F0) * pow(1.0 - NV, 5.0);
    col += amb * (alb * (1.0 - met) + Fa * 0.6);
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14);
    return pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2));
}

vec3 matcap() {
    vec3 N = normalize(vN), V = normalize(pc.cam.xyz - vW);
    if (dot(N, V) < 0.0) N = -N;
    vec3 n = vec3(dot(N, pc.mcR.xyz), dot(N, pc.mcU.xyz), dot(N, pc.mcB.xyz));
    vec3 L1 = normalize(vec3(.10, .50, .85)), L2 = normalize(vec3(-.60, -.30, .50));
    float d1 = max(dot(n, L1), 0.0), d2 = max(dot(n, L2), 0.0);
    float amb = .46 + .10 * n.y;
    vec3 c = vec3(.64) * (amb + .50 * d1 + .22 * d2);
    vec3 H = normalize(L1 + vec3(0.0, 0.0, 1.0));
    c += vec3(.04) * pow(max(dot(n, H), 0.0), 24.0);
    return clamp(c, 0.0, 1.0);
}

void main() {
    Op op = ops[vOp];
    float uL = op.a.x;
    vec3 uS = op.a.yzw;
    vec4 uC = op.col;
    if (uL < -2.5) {  // fundo do viewport (gradiente vertical)
        float t = clamp(1.0 - gl_FragCoord.y / max(pc.mcR.w, 1.0), 0.0, 1.0);
        vec3 top = vec3(.49, .56, .64), bot = vec3(.065, .07, .08);
        o = vec4(mix(bot, top, t), 1.0);
        return;
    }
    if (uL < -1.5) {  // losango
        vec2 q = (vP.xy - .5) * uS.xy;
        float dd = (abs(q.x) + abs(q.y) - uS.x * .5) * .7071;
        o = vec4(uC.rgb, uC.a * clamp(.5 - dd, 0.0, 1.0));
        return;
    }
    if (uL < 0.0) {  // retangulo arredondado
        vec2 q = (vP.xy - .5) * uS.xy;
        vec2 e = abs(q) - uS.xy * .5 + uS.z;
        float dd = length(max(e, 0.0)) + min(max(e.x, e.y), 0.0) - uS.z;
        o = vec4(uC.rgb, uC.a * clamp(.5 - dd, 0.0, 1.0));
        return;
    }
    if (uL > 0.5) {  // malha sombreada
        o = vec4(pc.cam.w > 0.5 ? matcap() : pbr(uC, op.b.xy), uC.a);
        return;
    }
    o = vec4(uC.rgb, uC.a);
}
