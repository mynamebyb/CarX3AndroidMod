//
//  mod.cpp - empty shell test
//
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>

#define LOG_TAG "CarX3Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static void* mod_thread(void* arg) {
    LOGI("mod thread alive, sleeping 30s...");
    sleep(30);
    LOGI("woke up, doing nothing");
    return nullptr;
}

__attribute__((constructor))
static void mod_init() {
    LOGI("mod .so loaded (constructor)");
    pthread_t tid;
    pthread_create(&tid, nullptr, mod_thread, nullptr);
    pthread_detach(tid);
}
