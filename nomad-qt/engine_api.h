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
    // tela de render
    bool anyKeys = false;          // algum objeto da cena tem keyframes
    int keyFirst = 0, keyLast = 0; // primeiro e ultimo key (frames da timeline, 24/s)
    bool rendering = false;
    float progress = 0;
    bool openRender = false;       // o menu pediu para abrir a tela de render
    int previewSeq = 0;
    std::string previewPath;
    std::vector<float> keys;  // keyframes do objeto selecionado
};

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
