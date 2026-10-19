#pragma once
#include <cstdint>
#include <vector>

// Primitivas procedurais (sem dependencia de Qt). Todas cabem em ~1 unidade.
enum PrimType { PrimCube = 0, PrimSphere, PrimCylinder, PrimCone, PrimTorus, PrimPlane, PrimCount };

struct MeshData {
    std::vector<float> v;      // interleaved: px py pz nx ny nz
    std::vector<uint16_t> i;   // triangulos
};

MeshData makeMesh(int type);
void primHalfExtents(int type, float out[3]);
// Textura matcap RGBA8 (N*N*4 bytes), ordem OpenGL (linha 0 = embaixo).
std::vector<uint8_t> makeMatcap(int n);
