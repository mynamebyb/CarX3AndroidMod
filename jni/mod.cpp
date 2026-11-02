//
//  mod.cpp - CarX3 Drift Points Multiplier (Android arm64)
//
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#define LOG_TAG "CarX3Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef void* Il2CppClass;
typedef void* Il2CppObject;
typedef void* Il2CppDomain;
typedef void* Il2CppThread;

static Il2CppDomain* (*il2cpp_domain_get)(void) = nullptr;
static Il2CppThread* (*il2cpp_thread_attach)(Il2CppDomain*) = nullptr;
static Il2CppClass* (*il2cpp_class_from_name)(void* image, const char* ns, const char* name) = nullptr;
static void* (*il2cpp_class_get_method_from_name)(Il2CppClass* klass, const char* name, int argsCount) = nullptr;
static void* (*il2cpp_runtime_invoke)(void* method, void* obj, void** params, void** exc) = nullptr;
static void* (*il2cpp_class_get_field_from_name)(Il2CppClass* klass, const char* name) = nullptr;
static void (*il2cpp_field_get_value)(void* obj, void* field, void* value) = nullptr;
static void (*il2cpp_field_set_value)(void* obj, void* field, void* value) = nullptr;
static void* (*il2cpp_domain_assembly_open)(Il2CppDomain* domain, const char* name) = nullptr;
static void* (*il2cpp_assembly_get_image)(void* assembly) = nullptr;

static void* load_il2cpp_api() {
    void* handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (!handle) return nullptr;
    il2cpp_domain_get = (decltype(il2cpp_domain_get))dlsym(handle, "il2cpp_domain_get");
    il2cpp_thread_attach = (decltype(il2cpp_thread_attach))dlsym(handle, "il2cpp_thread_attach");
    il2cpp_class_from_name = (decltype(il2cpp_class_from_name))dlsym(handle, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (decltype(il2cpp_class_get_method_from_name))dlsym(handle, "il2cpp_class_get_method_from_name");
    il2cpp_runtime_invoke = (decltype(il2cpp_runtime_invoke))dlsym(handle, "il2cpp_runtime_invoke");
    il2cpp_class_get_field_from_name = (decltype(il2cpp_class_get_field_from_name))dlsym(handle, "il2cpp_class_get_field_from_name");
    il2cpp_field_get_value = (decltype(il2cpp_field_get_value))dlsym(handle, "il2cpp_field_get_value");
    il2cpp_field_set_value = (decltype(il2cpp_field_set_value))dlsym(handle, "il2cpp_field_set_value");
    il2cpp_domain_assembly_open = (decltype(il2cpp_domain_assembly_open))dlsym(handle, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (decltype(il2cpp_assembly_get_image))dlsym(handle, "il2cpp_assembly_get_image");
    if (!il2cpp_domain_get || !il2cpp_class_from_name) return nullptr;
    LOGI("il2cpp API loaded");
    return handle;
}

static void* mod_thread(void* arg) {
    void* handle = nullptr;
    for (int i = 0; i < 120; i++) {
        handle = load_il2cpp_api();
        if (handle) break;
        sleep(1);
    }
    if (!handle) { LOGE("no il2cpp"); return nullptr; }

    Il2CppDomain* domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);

    void* assembly = il2cpp_domain_assembly_open(domain, "Assembly-CSharp");
    if (!assembly) { LOGE("no Assembly-CSharp"); return nullptr; }
    void* image = il2cpp_assembly_get_image(assembly);

    Il2CppClass* cls = il2cpp_class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel: %p", cls);

    void* f_speed = il2cpp_class_get_field_from_name(cls, "<pointsEarnSpeed>k__BackingField");
    void* f_combo = il2cpp_class_get_field_from_name(cls, "<comboMultiplier>k__BackingField");
    LOGI("fields: speed=%p combo=%p", f_speed, f_combo);

    Il2CppClass* rcls = il2cpp_class_from_name(image, "", "AmpRaceRewardsModel");
    LOGI("AmpRaceRewardsModel: %p", rcls);

    LOGI("=== Mod ready ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("CarX3Mod .so loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
