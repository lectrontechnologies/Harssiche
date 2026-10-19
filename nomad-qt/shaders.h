#pragma once

// Malha com matcap (sem PBR): a cor vem so da normal em espaco de vista.
static const char *kMeshVS = R"GLSL(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
uniform mat4 uMVP;
uniform mat3 uNrmMat;
out vec3 vN;
void main() {
    vN = uNrmMat * aNrm;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char *kMeshFS = R"GLSL(#version 300 es
precision highp float;
in vec3 vN;
uniform sampler2D uMatcap;
uniform vec3 uTint;
uniform float uSel;
uniform float uFlip;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vN);
    if (uFlip > 0.5 && !gl_FrontFacing) n = -n;
    vec2 uv = clamp(n.xy * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = texture(uMatcap, uv).rgb * uTint;
    float rim = pow(clamp(1.0 - abs(n.z), 0.0, 1.0), 2.5);
    c = mix(c, vec3(1.0, 0.62, 0.10), uSel * clamp(rim * 1.4, 0.0, 0.85));
    fragColor = vec4(c, 1.0);
}
)GLSL";

static const char *kLineVS = R"GLSL(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char *kLineFS = R"GLSL(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
}
)GLSL";
