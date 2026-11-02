//
//  mod.cpp - CarX3 Mod (debug: find methods, no hook yet)
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
typedef void* (*class_get_method_fn)(void*, const char*, int);
typedef void* (*assembly_open_fn)(void*, const char*);
typedef void* (*assembly_get_image_fn)(void*);
typedef uintptr_t (*method_get_pointer_fn)(void*);

typedef const char* (*method_get_name_fn)(void*);
typedef int (*class_num_methods_fn)(void*);
typedef void* (*class_get_method_fn_by_idx)(void*, int);

static void* mod_thread(void* arg) {
    LOGI("mod thread, sleep 20s...");
    sleep(20);

    void* il2cpp = dlopen("libil2cpp.so", RTLD_NOW);
    if (!il2cpp) { LOGE("no libil2cpp"); return nullptr; }

    auto domain_get = (domain_get_fn)dlsym(il2cpp, "il2cpp_domain_get");
    auto thread_attach = (thread_attach_fn)dlsym(il2cpp, "il2cpp_thread_attach");
    auto class_from_name = (class_from_name_fn)dlsym(il2cpp, "il2cpp_class_from_name");
    auto class_get_method = (class_get_method_fn)dlsym(il2cpp, "il2cpp_class_get_method_from_name");
    auto assembly_open = (assembly_open_fn)dlsym(il2cpp, "il2cpp_domain_assembly_open");
    auto assembly_get_image = (assembly_get_image_fn)dlsym(il2cpp, "il2cpp_assembly_get_image");
    auto method_get_ptr = (method_get_pointer_fn)dlsym(il2cpp, "il2cpp_method_get_pointer");
    auto method_get_name = (method_get_name_fn)dlsym(il2cpp, "il2cpp_method_get_name");
    auto class_num_methods = (class_num_methods_fn)dlsym(il2cpp, "il2cpp_class_num_methods");
    auto class_get_method_idx = (class_get_method_fn_by_idx)dlsym(il2cpp, "il2cpp_class_get_methods");

    if (!domain_get || !class_from_name) { LOGE("symbols missing"); return nullptr; }

    void* domain = domain_get();
    thread_attach(domain);
    LOGI("attached");

    void* asm_ = assembly_open(domain, "Assembly-CSharp");
    void* image = assembly_get_image(asm_);

    // DriftPointsModel
    void* cls = class_from_name(image, "", "DriftPointsModel");
    LOGI("DriftPointsModel = %p", cls);

    // 列出所有方法
    if (cls && class_num_methods && method_get_name) {
        int n = class_num_methods(cls);
        LOGI("DriftPointsModel has %d methods:", n);
        for (int i = 0; i < n && i < 60; i++) {
            void* m = class_get_method_idx(cls, i);
            const char* name = method_get_name(m);
            uintptr_t ptr = method_get_ptr ? method_get_ptr(m) : 0;
            LOGI("  [%d] %s @ 0x%lx", i, name ? name : "?", ptr);
        }
    }

    // 测试找 setter
    void* m_speed = class_get_method(cls, "set_pointsEarnSpeed", 1);
    void* m_combo = class_get_method(cls, "set_comboMultiplier", 1);
    LOGI("set_pointsEarnSpeed = %p", m_speed);
    LOGI("set_comboMultiplier = %p", m_combo);
    if (m_speed && method_get_ptr) LOGI("  -> impl @ 0x%lx", method_get_ptr(m_speed));
    if (m_combo && method_get_ptr) LOGI("  -> impl @ 0x%lx", method_get_ptr(m_combo));

    LOGI("=== SCAN DONE, no hooks ===");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
