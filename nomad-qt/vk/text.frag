#version 450
layout(set = 0, binding = 0) uniform sampler2D uTex;
layout(push_constant) uniform PC {
    vec2 scr;
    vec2 pad;
    vec4 col;
} pc;
layout(location = 0) in vec2 vT;
layout(location = 0) out vec4 o;
void main() {
    o = vec4(pc.col.rgb, pc.col.a * texture(uTex, vT).r);
}
