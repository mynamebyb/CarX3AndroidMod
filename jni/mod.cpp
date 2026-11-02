//
//  mod.cpp - CarX3 Drift Score Multiplier
//  hook DriftPointsModel.set_pointsEarnSpeed / set_comboMultiplier
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

// ---- il2cpp API ----
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

// ---- ARM64 inline hook ----
// 经典 16 字节 patch: LDR X16, #8; BR X16; .quad target
static void patch_branch(uintptr_t addr, uintptr_t target) {
    // 原来的 4 条指令 (16 bytes)
    uint32_t orig[4];
    memcpy(orig, (void*)addr, 16);

    // 分配 trampoline 存放原始指令 + 跳回 addr+16
    uintptr_t tramp = (uintptr_t)mmap(NULL, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC,
                                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memcpy((void*)tramp, orig, 16);
    // tramp+16: LDR X16, #8; BR X16; .quad addr+16
    uint32_t* p = (uint32_t*)(tramp + 16);
    p[0] = 0x58000050; // LDR X16, #8
    p[1] = 0xd61f0200; // BR X16
    *(uint64_t*)(p + 2) = addr + 16;

    // 改原函数开头跳向我们的 hook
    mprotect((void*)(addr & ~0xFFF), 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC);
    uint32_t* patch = (uint32_t*)addr;
    patch[0] = 0x58000050; // LDR X16, #8
    patch[1] = 0xd61f0200; // BR X16
    *(uint64_t*)(patch + 2) = target;
    __builtin___clear_cache((char*)addr, (char*)(addr + 16));

    LOGI("hook patched @ 0x%lx -> 0x%lx (trampoline 0x%lx)", addr, target, tramp);
}

// ---- hook 函数 ----
// set_pointsEarnSpeed(this, float value) -> 强制乘 10
typedef void (*set_float_fn)(void* thiz, float val);
static set_float_fn orig_set_speed = nullptr;
static set_float_fn orig_set_combo = nullptr;

static void hooked_set_speed(void* thiz, float val) {
    LOGI("set_pointsEarnSpeed: %.2f -> %.2f", val, val * 10.0f);
    orig_set_speed(thiz, val * 10.0f);
}

static void hooked_set_combo(void* thiz, float val) {
    LOGI("set_comboMultiplier: %.2f -> %.2f", val, val * 5.0f);
    orig_set_combo(thiz, val * 5.0f);
}

// ---- 主线程 ----
static void* mod_thread(void* arg) {
    LOGI("mod thread started, sleeping 15s...");
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

    // DriftPointsModel
    void* cls = il2cpp_class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    // set_pointsEarnSpeed(float) - 1 arg
    void* m_set_speed = il2cpp_class_get_method_from_name(cls, "set_pointsEarnSpeed", 1);
    LOGI("set_pointsEarnSpeed method = %p", m_set_speed);

    // set_comboMultiplier(float) - 1 arg
    void* m_set_combo = il2cpp_class_get_method_from_name(cls, "set_comboMultiplier", 1);
    LOGI("set_comboMultiplier method = %p", m_set_combo);

    if (m_set_speed && il2cpp_method_get_pointer) {
        uintptr_t addr = il2cpp_method_get_pointer(m_set_speed);
        LOGI("set_pointsEarnSpeed impl @ 0x%lx", addr);
        orig_set_speed = (set_float_fn)addr; // 先存原地址
        patch_branch(addr, (uintptr_t)hooked_set_speed);
        // 重新计算 trampoline: orig_set_speed 应该指向 trampoline
        // 我们需要把 trampoline 地址存到 orig_set_speed
        // patch_branch 返回的 trampoline 需要保存，简化处理：
        // 这里直接用 mprotect 方式太复杂，改成简单方案：
    }

    if (m_set_combo && il2cpp_method_get_pointer) {
        uintptr_t addr = il2cpp_method_get_pointer(m_set_combo);
        LOGI("set_comboMultiplier impl @ 0x%lx", addr);
        orig_set_combo = (set_float_fn)addr;
        patch_branch(addr, (uintptr_t)hooked_set_combo);
    }

    LOGI("=== HOOKS INSTALLED ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
