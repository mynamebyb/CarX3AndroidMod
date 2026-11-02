#include <jni.h>
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
typedef void* (*class_get_method_fn)(void*, const char*, int);
typedef uintptr_t (*method_get_ptr_fn)(void*);
typedef const char* (*method_get_name_fn)(void*);

typedef void* (*domain_get_assemblies_fn)(void*, void*);
typedef const char* (*assembly_get_name_fn)(void*);
typedef void* (*assembly_get_image_fn)(void*);

typedef void (*set_float_fn)(void* thiz, float val);
static set_float_fn orig_set_speed = nullptr;
static set_float_fn orig_set_combo = nullptr;

// ARM64 inline hook
static uintptr_t do_hook(uintptr_t target, void* hook_fn) {
    uint32_t orig[4];
    memcpy(orig, (void*)target, 16);

    uintptr_t tramp = (uintptr_t)mmap(NULL, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC,
                                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memcpy((void*)tramp, orig, 16);
    uint32_t* p = (uint32_t*)(tramp + 16);
    p[0] = 0x58000050; // LDR X16, #8
    p[1] = 0xd61f0200; // BR X16
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
    LOGI("mod thread started, sleep 20s...");
    sleep(20);

    // 等 libil2cpp.so
    void* il2cpp = nullptr;
    for (int i = 0; i < 60; i++) {
        il2cpp = dlopen("libil2cpp.so", RTLD_NOW);
        if (il2cpp) break;
        sleep(1);
    }
    if (!il2cpp) { LOGE("no libil2cpp"); return nullptr; }
    LOGI("libil2cpp found");

    auto domain_get = (domain_get_fn)dlsym(il2cpp, "il2cpp_domain_get");
    auto thread_attach = (thread_attach_fn)dlsym(il2cpp, "il2cpp_thread_attach");
    auto class_from_name = (class_from_name_fn)dlsym(il2cpp, "il2cpp_class_from_name");
    auto class_get_method = (class_get_method_fn)dlsym(il2cpp, "il2cpp_class_get_method_from_name");
    auto method_get_ptr = (method_get_ptr_fn)dlsym(il2cpp, "il2cpp_method_get_pointer");

    if (!domain_get || !class_from_name) { LOGE("symbols missing"); return nullptr; }

    void* domain = domain_get();
    thread_attach(domain);
    LOGI("thread attached to il2cpp");

    // 枚举所有 assembly 找 Assembly-CSharp
    auto get_assemblies = (domain_get_assemblies_fn)dlsym(il2cpp, "il2cpp_domain_get_assemblies");
    auto asm_name = (assembly_get_name_fn)dlsym(il2cpp, "il2cpp_assembly_get_name");
    auto asm_get_image = (assembly_get_image_fn)dlsym(il2cpp, "il2cpp_assembly_get_image");

    void* image = nullptr;
    if (get_assemblies && asm_name && asm_get_image) {
        uint32_t count = 0;
        void** assemblies = (void**)get_assemblies(domain, &count);
        LOGI("found %u assemblies", count);
        for (uint32_t i = 0; i < count; i++) {
            const char* name = asm_name(assemblies[i]);
            if (name && strstr(name, "Assembly-CSharp")) {
                image = asm_get_image(assemblies[i]);
                LOGI("found Assembly-CSharp: %s", name);
                break;
            }
        }
    }
    if (!image) { LOGE("Assembly-CSharp not found"); return nullptr; }

    void* cls = class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    void* m_speed = class_get_method(cls, "set_pointsEarnSpeed", 1);
    void* m_combo = class_get_method(cls, "set_comboMultiplier", 1);
    LOGI("methods: speed=%p combo=%p", m_speed, m_combo);

    if (m_speed && method_get_ptr) {
        uintptr_t addr = method_get_ptr(m_speed);
        LOGI("set_pointsEarnSpeed @ 0x%lx", addr);
        orig_set_speed = (set_float_fn)do_hook(addr, (void*)hooked_set_speed);
    }
    if (m_combo && method_get_ptr) {
        uintptr_t addr = method_get_ptr(m_combo);
        LOGI("set_comboMultiplier @ 0x%lx", addr);
        orig_set_combo = (set_float_fn)do_hook(addr, (void*)hooked_set_combo);
    }

    LOGI("=== HOOKS DONE ===");
    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad, starting mod thread");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
    return JNI_VERSION_1_6;
}
