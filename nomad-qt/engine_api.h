#pragma once
#include <jni.h>
#include <string>
#include <vector>

// Evento ja traduzido do Qt para o formato que o motor entende.
struct EngEvent {
    int kind = 0;    // 0 toque, 1 texto, 2 backspace, 3 enter, 4 voltar, 5 comando (action = comando, x[0], x[1] = valores)
    int action = 0;  // acoes do Android: 0 DOWN, 1 UP, 2 MOVE, 3 CANCEL, 5 POINTER_DOWN, 6 POINTER_UP
    int n = 0;       // quantidade de dedos
    float x[2] = {0, 0};
    float y[2] = {0, 0};
    int cp = 0;      // caractere (kind == 1)
};

// Uma linha do outliner (um objeto da cena)
struct EngRow {
    std::string name;
    int type = 0;          // 0 malha, 1 camera, 2 luz, 3 osso
    int parent = -1;
    bool hide = false;     // olho: oculto no viewport e no render
    bool nor = false;      // camera: fora do render
    bool operator==(const EngRow &o) const {
        return name == o.name && type == o.type && parent == o.parent && hide == o.hide && nor == o.nor;
    }
};

// Estado da timeline lido pelo QML (frames em unidades de frame, 24 fps)
struct EngTimeline {
    float frame = 1;
    int fs = 1, fe = 240;  // intervalo global
    int vs = 1, ve = 120;  // janela visivel
    bool playing = false;
    int dir = 1;
    bool visible = true;   // falso no modo Escultura
    bool hasSel = false;
    int tool = 3;          // 0 mover, 1 rotacionar, 2 escalar, 3 selecionar
    bool canUndo = false, canRedo = false;
    bool sculpt = false;
    bool autoKey = false;  // Auto Key ligado
    // tela de render
    bool anyKeys = false;          // algum objeto da cena tem keyframes
    int keyFirst = 0, keyLast = 0; // primeiro e ultimo key (frames da timeline, 24/s)
    bool rendering = false;
    float progress = 0;
    bool openRender = false;       // o menu pediu para abrir a tela de render
    int previewSeq = 0;
    std::string previewPath;
    std::vector<float> keys;  // keyframes do objeto selecionado
    std::vector<EngRow> rows; // objetos da cena (outliner)
    int selObj = -1;
};

// ---- backend Vulkan: o motor grava as ordens de desenho e o renderizador Vulkan as executa ----
struct EngVkOp {
    int kind = 0;   // 0 malha, 1 texto, 2 recorte (scissor)
    int depth = 0;  // 1 = teste de profundidade ligado
    int lines = 0;  // 1 = lista de linhas (senao triangulos)
    int vid = 0;    // malha: id estavel (muda so se a malha for recriada)
    int n = 0;      // malha: numero de vertices
    const float *cpu = nullptr;  // malha: vertices (posicao + normal, 6 floats cada); valido ate o fim do quadro
    const float *uv = nullptr;   // malha com textura: 2 floats por vertice
    const float *skin = nullptr; // malha com esqueleto: 8 floats por vertice (4 indices de osso + 4 pesos); b[3] = primeira matriz da paleta
    float mvp[16] = {};
    float mod[16] = {};
    float col[4] = {};
    float a[4] = {};  // a[0] = lit (>0.5 sombreado, 0 chapado, -1 retangulo, -2 losango, -3 fundo), a[1..3] = escala do retangulo
    float b[4] = {};  // b[0] = metal, b[1] = rugosidade
    float tl[4] = {};
    int tfirst = 0, tcount = 0;  // texto: faixa de vertices (4 floats cada) em EngVkFrame::text
    float sc[4] = {};            // recorte: x, y, w, h em px (w <= 0 desliga)
};
// interface do ImGui (gizmos): listas de desenho concatenadas
struct EngVkGzCmd {
    float clip[4] = {};  // x0, y0, x1, y1 em px
    int vtxOff = 0, idxOff = 0, count = 0;
};
struct EngVkGz {
    int nv = 0;                          // vertices
    std::vector<unsigned char> vtx;      // ImDrawVert (20 bytes: x, y, u, v, cor RGBA8)
    std::vector<unsigned short> idx;
    std::vector<EngVkGzCmd> cmds;
};
constexpr int kEngVkMaxMats = 4096;  // limite de matrizes de osso por quadro
struct EngVkFrame {
    std::vector<EngVkOp> scene;  // 3D (por baixo do QML)
    std::vector<EngVkOp> ui;     // interface do motor (por cima do QML)
    std::vector<float> text;     // vertices de texto: x, y, u, v
    EngVkGz gz;                  // gizmos (ImGui)
    std::vector<float> pal;      // paletas de ossos: matrizes 4x4 (16 floats, colunas) uma apos a outra
    float cam[4] = {};           // xyz = posicao da camera, w = 1 se matcap
    float mcR[4] = {}, mcU[4] = {}, mcB[4] = {};  // base do matcap (w de mcR = altura da tela em px)
};
void eng_set_vk(bool on);                                // liga o modo Vulkan (antes de eng_init)
void eng_vk_frame(EngVkFrame &out, int w, int h);        // avanca o tempo, roda a cena e a interface e grava as ordens
int eng_vk_tex_count(int &size, unsigned &ver);          // texturas de material: camadas size x size RGBA8; ver muda quando entra uma nova
const unsigned char *eng_vk_tex_layer(int i);
const unsigned char *eng_vk_gz_atlas(int &w, int &h, int &bpp, unsigned &ver);  // atlas do ImGui (1 ou 4 bytes por pixel); ver muda quando o conteudo muda
const unsigned char *eng_vk_font(int &w, int &h);        // atlas da fonte (R8), ou nullptr

// ---- motor (engine.cpp) - tudo isto roda na thread de render ----
void eng_set_android(JavaVM *vm, jobject activity);
bool eng_ready();
void eng_init(const char *dir, int w, int h);
void eng_resize(int w, int h);
void eng_event(const EngEvent &e);
void eng_frame();
void eng_gl_lost();
bool eng_dialog_open();
void eng_timeline(EngTimeline &t);
void eng_frame_ui();                       // fase da interface do motor (depois do Qt Quick)
void eng_ui_rects(std::vector<float> &out);  // retangulos x,y,w,h (px) ocupados pela interface do motor

// ---- plataforma (engineview.cpp) - chamadas pelo motor ----
// backend Vulkan: renderiza as ordens gravadas fora da tela e devolve os pixels (RGBA8, de baixo para cima).
// 0 = ok, 1 = ainda nao da (tenta no proximo quadro), 2 = indisponivel
int plat_vk_capture(const EngVkFrame &f, int w, int h, std::vector<unsigned char> &px);
void plat_show_keyboard();
void plat_hide_keyboard();
void plat_keep_screen_on(bool on);
