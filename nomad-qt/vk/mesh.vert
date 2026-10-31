#version 450
// Malhas do motor: posicao + normal; os dados por objeto (matrizes, cor, parametros) vem de um buffer de
// armazenamento indexado por gl_InstanceIndex (= indice da ordem de desenho).
layout(location = 0) in vec3 aP;
layout(location = 1) in vec3 aN;
layout(location = 2) in vec4 aJ;  // so em malhas com esqueleto: indices de osso
layout(location = 3) in vec4 aW;  // pesos

struct Op {
    mat4 mvp;
    mat4 mod;
    vec4 col;
    vec4 a;   // a.x = lit, a.yzw = escala (retangulos)
    vec4 b;   // b.xy = metal, rugosidade, b.z = esqueleto ligado, b.w = primeira matriz da paleta
    vec4 tl;
};
layout(std430, set = 0, binding = 0) readonly buffer Ops { Op ops[]; };
layout(std430, set = 0, binding = 1) readonly buffer Pal { mat4 pal[]; };

layout(location = 0) out vec3 vN;
layout(location = 1) out vec3 vP;
layout(location = 2) out vec3 vW;
layout(location = 3) flat out int vOp;

void main() {
    Op o = ops[gl_InstanceIndex];
    vec4 p = vec4(aP, 1.0);
    vec3 n = aN;
    if (o.b.z > 0.5) {  // skinning: mesma formula do motor (a matriz do objeto e identidade nesse caso)
        int base = int(o.b.w);
        mat4 S = aW.x * pal[base + int(aJ.x)] + aW.y * pal[base + int(aJ.y)] + aW.z * pal[base + int(aJ.z)] + aW.w * pal[base + int(aJ.w)];
        p = S * p;
        n = mat3(S) * n;
    }
    vec4 c = o.mvp * p;
    // as matrizes do motor sao de OpenGL (y para cima, z de -w a w): converte para o clip do Vulkan
    c.y = -c.y;
    c.z = (c.z + c.w) * 0.5;
    gl_Position = c;
    vN = mat3(o.mod) * n;
    vP = aP;
    vW = (o.mod * p).xyz;
    vOp = gl_InstanceIndex;
}
