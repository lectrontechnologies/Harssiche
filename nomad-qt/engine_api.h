#pragma once
#include <jni.h>

// Evento ja traduzido do Qt para o formato que o motor entende.
struct EngEvent {
    int kind = 0;    // 0 toque, 1 texto, 2 backspace, 3 enter, 4 voltar
    int action = 0;  // acoes do Android: 0 DOWN, 1 UP, 2 MOVE, 3 CANCEL, 5 POINTER_DOWN, 6 POINTER_UP
    int n = 0;       // quantidade de dedos
    float x[2] = {0, 0};
    float y[2] = {0, 0};
    int cp = 0;      // caractere (kind == 1)
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

// ---- plataforma (engineview.cpp) - chamadas pelo motor ----
void plat_show_keyboard();
void plat_hide_keyboard();
void plat_keep_screen_on(bool on);
