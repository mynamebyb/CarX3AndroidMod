//
//  mod.cpp - CarX3 Mod (safe minimal version)
//
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "CarX3Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef void* (*domain_get_fn)(void);
typedef void* (*thread_attach_fn)(void*);
typedef void* (*class_from_name_fn)(void*, const char*, const char*);
typedef void* (*class_get_field_fn)(void*, const char*);
typedef void* (*assembly_open_fn)(void*, const char*);
typedef void* (*assembly_get_image_fn)(void*);

static domain_get_fn il2cpp_domain_get = nullptr;
static thread_attach_fn il2cpp_thread_attach = nullptr;
static class_from_name_fn il2cpp_class_from_name = nullptr;
static class_get_field_fn il2cpp_class_get_field_from_name = nullptr;
static assembly_open_fn il2cpp_domain_assembly_open = nullptr;
static assembly_get_image_fn il2cpp_assembly_get_image = nullptr;

static void* mod_thread(void* arg) {
    LOGI("mod thread started, sleeping 15s...");
    sleep(15);

    void* il2cpp = nullptr;
    for (int i = 0; i < 60; i++) {
        il2cpp = dlopen("libil2cpp.so", RTLD_NOW);
        if (il2cpp) break;
        sleep(1);
    }
    if (!il2cpp) { LOGE("libil2cpp.so not found"); return nullptr; }
    LOGI("libil2cpp.so loaded");

    il2cpp_domain_get = (domain_get_fn)dlsym(il2cpp, "il2cpp_domain_get");
    il2cpp_thread_attach = (thread_attach_fn)dlsym(il2cpp, "il2cpp_thread_attach");
    il2cpp_class_from_name = (class_from_name_fn)dlsym(il2cpp, "il2cpp_class_from_name");
    il2cpp_class_get_field_from_name = (class_get_field_fn)dlsym(il2cpp, "il2cpp_class_get_field_from_name");
    il2cpp_domain_assembly_open = (assembly_open_fn)dlsym(il2cpp, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (assembly_get_image_fn)dlsym(il2cpp, "il2cpp_assembly_get_image");

    if (!il2cpp_domain_get || !il2cpp_class_from_name) {
        LOGE("il2cpp symbols missing");
        return nullptr;
    }
    LOGI("il2cpp symbols resolved");

    void* domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
    LOGI("thread attached");

    void* asm_ = il2cpp_domain_assembly_open(domain, "Assembly-CSharp");
    if (!asm_) { LOGE("Assembly-CSharp not found"); return nullptr; }
    void* image = il2cpp_assembly_get_image(asm_);

    void* cls = il2cpp_class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    void* rcls = il2cpp_class_from_name(image, "", "AmpRaceRewardsModel");
    LOGI("AmpRaceRewardsModel = %p", rcls);

    LOGI("=== mod init done ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
