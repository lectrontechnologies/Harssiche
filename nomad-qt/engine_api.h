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
    float mvp[16] = {};
    float mod[16] = {};
    float col[4] = {};
    float a[4] = {};  // a[0] = lit (>0.5 sombreado, 0 chapado, -1 retangulo, -2 losango, -3 fundo), a[1..3] = escala do retangulo
    float b[4] = {};  // b[0] = metal, b[1] = rugosidade
    float tl[4] = {};
    int tfirst = 0, tcount = 0;  // texto: faixa de vertices (4 floats cada) em EngVkFrame::text
    float sc[4] = {};            // recorte: x, y, w, h em px (w <= 0 desliga)
};
struct EngVkFrame {
    std::vector<EngVkOp> scene;  // 3D (por baixo do QML)
    std::vector<EngVkOp> ui;     // interface do motor (por cima do QML)
    std::vector<float> text;     // vertices de texto: x, y, u, v
    float cam[4] = {};           // xyz = posicao da camera, w = 1 se matcap
    float mcR[4] = {}, mcU[4] = {}, mcB[4] = {};  // base do matcap (w de mcR = altura da tela em px)
};
void eng_set_vk(bool on);                                // liga o modo Vulkan (antes de eng_init)
void eng_vk_frame(EngVkFrame &out, int w, int h);        // avanca o tempo, roda a cena e a interface e grava as ordens
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
void plat_show_keyboard();
void plat_hide_keyboard();
void plat_keep_screen_on(bool on);
