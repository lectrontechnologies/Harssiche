#version 450
// Texto: vertices (x, y em px; u, v) em um buffer sequencial; a cor e o tamanho da tela vem por push constants.
layout(location = 0) in vec4 aV;
layout(push_constant) uniform PC {
    vec2 scr;
    vec2 pad;
    vec4 col;
} pc;
layout(location = 0) out vec2 vT;
void main() {
    gl_Position = vec4(aV.x / pc.scr.x * 2.0 - 1.0, aV.y / pc.scr.y * 2.0 - 1.0, 0.0, 1.0);
    vT = aV.zw;
}
