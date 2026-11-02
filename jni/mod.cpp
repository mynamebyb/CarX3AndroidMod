//
//  mod.cpp - CarX3 Mod (safe minimal version)
//  JNI_OnLoad/constructor 极简，立刻返回，不碰主线程
//  子线程延时等待 il2cpp 就绪后再操作
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

typedef void* Il2CppDomain;
typedef void* Il2CppThread;
typedef void* Il2CppClass;

static Il2CppDomain* (*il2cpp_domain_get)(void) = nullptr;
static Il2CppThread* (*il2cpp_thread_attach)(Il2CppDomain*) = nullptr;
static Il2CppClass* (*il2cpp_class_from_name)(void* image, const char* ns, const char* name) = nullptr;
static void* (*il2cpp_class_get_field_from_name)(Il2CppClass* klass, const char* name) = nullptr;
static void* (*il2cpp_domain_assembly_open)(Il2CppDomain* domain, const char* name) = nullptr;
static void* (*il2cpp_assembly_get_image)(void* assembly) = nullptr;

static void* mod_thread(void* arg) {
    LOGI("mod thread started, sleeping 15s...");
    sleep(15);  // 等游戏完全启动

    // 等 libil2cpp.so 可用
    void* il2cpp = nullptr;
    for (int i = 0; i < 60; i++) {
        il2cpp = dlopen("libil2cpp.so", RTLD_NOW);
        if (il2cpp) break;
        sleep(1);
    }
    if (!il2cpp) { LOGE("libil2cpp.so not found"); return nullptr; }
    LOGI("libil2cpp.so loaded");

    // 拿函数指针
    il2cpp_domain_get = (void*(*)(void))dlsym(il2cpp, "il2cpp_domain_get");
    il2cpp_thread_attach = (void*(*)(void*))dlsym(il2cpp, "il2cpp_thread_attach");
    il2cpp_class_from_name = (void*(*)(void*, const char*, const char*))dlsym(il2cpp, "il2cpp_class_from_name");
    il2cpp_class_get_field_from_name = (void*(*)(void*, const char*))dlsym(il2cpp, "il2cpp_class_get_field_from_name");
    il2cpp_domain_assembly_open = (void*(*)(void*, const char*))dlsym(il2cpp, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (void*(*)(void*))dlsym(il2cpp, "il2cpp_assembly_get_image");

    if (!il2cpp_domain_get || !il2cpp_class_from_name) {
        LOGE("il2cpp symbols missing");
        return nullptr;
    }
    LOGI("il2cpp symbols resolved");

    Il2CppDomain* domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
    LOGI("thread attached to il2cpp");

    void* asm_ = il2cpp_domain_assembly_open(domain, "Assembly-CSharp");
    if (!asm_) { LOGE("Assembly-CSharp not found"); return nullptr; }
    void* image = il2cpp_assembly_get_image(asm_);

    Il2CppClass* cls = il2cpp_class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    Il2CppClass* rcls = il2cpp_class_from_name(image, "", "AmpRaceRewardsModel");
    LOGI("AmpRaceRewardsModel = %p", rcls);

    LOGI("=== mod init done ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded (constructor)");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
    // 立刻返回，不在主线程做任何事
}
