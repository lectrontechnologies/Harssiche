#version 450
// Malhas do motor: posicao + normal; os dados por objeto (matrizes, cor, parametros) vem de um buffer de
// armazenamento indexado por gl_InstanceIndex (= indice da ordem de desenho).
layout(location = 0) in vec3 aP;
layout(location = 1) in vec3 aN;

struct Op {
    mat4 mvp;
    mat4 mod;
    vec4 col;
    vec4 a;   // a.x = lit, a.yzw = escala (retangulos)
    vec4 b;   // b.xy = metal, rugosidade
    vec4 tl;
};
layout(std430, set = 0, binding = 0) readonly buffer Ops { Op ops[]; };

layout(location = 0) out vec3 vN;
layout(location = 1) out vec3 vP;
layout(location = 2) out vec3 vW;
layout(location = 3) flat out int vOp;

void main() {
    Op o = ops[gl_InstanceIndex];
    vec4 p = vec4(aP, 1.0);
    vec4 c = o.mvp * p;
    // as matrizes do motor sao de OpenGL (y para cima, z de -w a w): converte para o clip do Vulkan
    c.y = -c.y;
    c.z = (c.z + c.w) * 0.5;
    gl_Position = c;
    vN = mat3(o.mod) * aN;
    vP = aP;
    vW = (o.mod * p).xyz;
    vOp = gl_InstanceIndex;
}
