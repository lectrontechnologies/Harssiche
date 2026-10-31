#version 450
// Interface do ImGui (gizmos): vertices em pixels, origem no canto superior esquerdo.
layout(location = 0) in vec2 aP;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aC;
layout(push_constant) uniform PC { vec2 scale; } pc;  // 2/largura, 2/altura
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vC;
void main() {
    gl_Position = vec4(aP * pc.scale - vec2(1.0), 0.0, 1.0);
    vUV = aUV;
    vC = aC;
}
