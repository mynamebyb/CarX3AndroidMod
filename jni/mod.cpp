//
//  mod.cpp - CarX3 Drift Score Multiplier
//
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#define LOG_TAG "CarX3Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef void* (*domain_get_fn)(void);
typedef void* (*thread_attach_fn)(void*);
typedef void* (*class_from_name_fn)(void*, const char*, const char*);
typedef void* (*class_get_method_fn)(void*, const char*, int);
typedef void* (*assembly_open_fn)(void*, const char*);
typedef void* (*assembly_get_image_fn)(void*);
typedef uintptr_t (*method_get_pointer_fn)(void*);

static domain_get_fn il2cpp_domain_get;
static thread_attach_fn il2cpp_thread_attach;
static class_from_name_fn il2cpp_class_from_name;
static class_get_method_fn il2cpp_class_get_method_from_name;
static assembly_open_fn il2cpp_domain_assembly_open;
static assembly_get_image_fn il2cpp_assembly_get_image;
static method_get_pointer_fn il2cpp_method_get_pointer;

typedef void (*set_float_fn)(void* thiz, float val);
static set_float_fn orig_set_speed = nullptr;
static set_float_fn orig_set_combo = nullptr;

// 返回 trampoline 地址（调用它=执行原函数）
static uintptr_t do_hook(uintptr_t target, void* hook_fn) {
    uint32_t orig[4];
    memcpy(orig, (void*)target, 16);

    uintptr_t tramp = (uintptr_t)mmap(NULL, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC,
                                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memcpy((void*)tramp, orig, 16);
    uint32_t* p = (uint32_t*)(tramp + 16);
    p[0] = 0x58000050;
    p[1] = 0xd61f0200;
    *(uint64_t*)(p + 2) = target + 16;

    mprotect((void*)(target & ~0xFFF), 0x2000, PROT_READ|PROT_WRITE|PROT_EXEC);
    uint32_t* patch = (uint32_t*)target;
    patch[0] = 0x58000050;
    patch[1] = 0xd61f0200;
    *(uint64_t*)(patch + 2) = (uint64_t)hook_fn;
    __builtin___clear_cache((char*)target, (char*)(target + 16));

    LOGI("hook @ 0x%lx -> %p, tramp=0x%lx", target, hook_fn, tramp);
    return tramp;
}

static void hooked_set_speed(void* thiz, float val) {
    LOGI("speed: %.1f -> %.1f", val, val * 10.0f);
    orig_set_speed(thiz, val * 10.0f);
}

static void hooked_set_combo(void* thiz, float val) {
    LOGI("combo: %.1f -> %.1f", val, val * 5.0f);
    orig_set_combo(thiz, val * 5.0f);
}

static void* mod_thread(void* arg) {
    LOGI("mod thread, sleep 15s...");
    sleep(15);

    void* il2cpp = nullptr;
    for (int i = 0; i < 60; i++) {
        il2cpp = dlopen("libil2cpp.so", RTLD_NOW);
        if (il2cpp) break;
        sleep(1);
    }
    if (!il2cpp) { LOGE("no libil2cpp"); return nullptr; }

    il2cpp_domain_get = (domain_get_fn)dlsym(il2cpp, "il2cpp_domain_get");
    il2cpp_thread_attach = (thread_attach_fn)dlsym(il2cpp, "il2cpp_thread_attach");
    il2cpp_class_from_name = (class_from_name_fn)dlsym(il2cpp, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (class_get_method_fn)dlsym(il2cpp, "il2cpp_class_get_method_from_name");
    il2cpp_domain_assembly_open = (assembly_open_fn)dlsym(il2cpp, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (assembly_get_image_fn)dlsym(il2cpp, "il2cpp_assembly_get_image");
    il2cpp_method_get_pointer = (method_get_pointer_fn)dlsym(il2cpp, "il2cpp_method_get_pointer");

    if (!il2cpp_domain_get || !il2cpp_class_from_name) { LOGE("symbols missing"); return nullptr; }

    void* domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);

    void* asm_ = il2cpp_domain_assembly_open(domain, "Assembly-CSharp");
    void* image = il2cpp_assembly_get_image(asm_);

    void* cls = il2cpp_class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    void* m_speed = il2cpp_class_get_method_from_name(cls, "set_pointsEarnSpeed", 1);
    void* m_combo = il2cpp_class_get_method_from_name(cls, "set_comboMultiplier", 1);
    LOGI("methods: speed=%p combo=%p", m_speed, m_combo);

    if (m_speed) {
        uintptr_t addr = il2cpp_method_get_pointer(m_speed);
        orig_set_speed = (set_float_fn)do_hook(addr, (void*)hooked_set_speed);
    }
    if (m_combo) {
        uintptr_t addr = il2cpp_method_get_pointer(m_combo);
        orig_set_combo = (set_float_fn)do_hook(addr, (void*)hooked_set_combo);
    }

    LOGI("=== HOOKS DONE ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
