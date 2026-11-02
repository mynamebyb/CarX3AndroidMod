#include <jni.h>
#include <android/log.h>

#define LOG_TAG "CarX3Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad called - empty mod");
    return JNI_VERSION_1_6;
}
