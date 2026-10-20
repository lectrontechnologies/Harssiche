#pragma once
// Substitui o android_native_app_glue: o motor continua usando gApp->activity (JNI),
// mas agora a atividade e a JavaVM vem do Qt.
#include <jni.h>
#include <android/log.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <EGL/egl.h>

#include "engine_api.h"

// A thread de render pertence ao Qt: nunca desanexar da JVM.
struct ShimVM {
    JavaVM *vm = nullptr;
    jint AttachCurrentThread(JNIEnv **env, void *) {
        if (vm->GetEnv(reinterpret_cast<void **>(env), JNI_VERSION_1_6) == JNI_OK) return JNI_OK;
        return vm->AttachCurrentThread(env, nullptr);
    }
    jint GetEnv(void **env, jint version) { return vm->GetEnv(env, version); }
    jint DetachCurrentThread() { return JNI_OK; }
};

struct ShimActivity {
    ShimVM *vm = nullptr;
    jobject clazz = nullptr;
};

struct android_app {
    ShimActivity *activity = nullptr;
};

// Evento de entrada no lugar do AInputEvent. type: 1 = tecla, 2 = toque.
struct ShimEvt {
    int type = 0;
    int action = 0;
    int n = 0;
    float x[2] = {0, 0};
    float y[2] = {0, 0};
    int keyAction = 0;
    int keyCode = 0;
    int meta = 0;
};
