#version 450
// Fundo (gradiente de ceu) + grade do chao (y = 0) com eixos X (vermelho) e Z (azul).
// A camera chega por push constants: eye.w = tan(fov/2), right.w = proporcao da tela.
layout(push_constant) uniform PC {
    vec4 eye;
    vec4 right;
    vec4 up;
    vec4 fwd;
} pc;
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;

void main() {
    float th = pc.eye.w;
    float asp = pc.right.w;
    // y do framebuffer cresce para baixo: o topo da tela e ndc.y = -1
    vec3 rd = normalize(pc.fwd.xyz + vNdc.x * asp * th * pc.right.xyz - vNdc.y * th * pc.up.xyz);
    vec3 ro = pc.eye.xyz;

    vec3 horizon = vec3(0.70, 0.71, 0.76);
    vec3 zenith = vec3(0.60, 0.65, 0.80);
    vec3 below = vec3(0.60, 0.60, 0.64);
    vec3 col = mix(horizon, zenith, clamp(rd.y * 1.6, 0.0, 1.0));
    col = mix(col, below, clamp(-rd.y * 3.0, 0.0, 1.0));

    // interseccao com o plano y = 0 (derivadas calculadas fora de ramificacoes)
    bool hit = (rd.y < -1e-4) && (ro.y > 0.0);
    float t = hit ? (-ro.y / rd.y) : 1.0e4;
    vec2 g = (ro + rd * t).xz;
    vec2 d = max(fwidth(g), vec2(1.0e-6));

    vec2 gm = abs(fract(g - 0.5) - 0.5) / d;
    float lnMinor = 1.0 - clamp(min(gm.x, gm.y), 0.0, 1.0);
    vec2 g10 = g / 10.0;
    vec2 gM = abs(fract(g10 - 0.5) - 0.5) / (d / 10.0);
    float lnMajor = 1.0 - clamp(min(gM.x, gM.y), 0.0, 1.0);
    float axX = 1.0 - clamp(abs(g.y) / d.y, 0.0, 1.0);  // eixo X (z = 0)
    float axZ = 1.0 - clamp(abs(g.x) / d.x, 0.0, 1.0);  // eixo Z (x = 0)

    float fade = hit ? clamp(1.0 - t / 90.0, 0.0, 1.0) * clamp(-rd.y * 14.0, 0.0, 1.0) : 0.0;
    col = mix(col, vec3(0.30, 0.31, 0.36), lnMinor * 0.30 * fade);
    col = mix(col, vec3(0.22, 0.23, 0.30), lnMajor * 0.50 * fade);
    col = mix(col, vec3(0.85, 0.25, 0.25), axX * 0.85 * fade);
    col = mix(col, vec3(0.25, 0.40, 0.90), axZ * 0.85 * fade);
    outColor = vec4(col, 1.0);
}
