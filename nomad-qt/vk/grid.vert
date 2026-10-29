#version 450
// Triangulo que cobre a tela inteira (sem buffer de vertices).
layout(location = 0) out vec2 vNdc;
void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vNdc = p * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 0.0, 1.0);
}
