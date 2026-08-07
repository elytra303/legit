#include <windows.h>
#include <jni.h>
#include "gl_hook.h"
#include "minecraft.h"

extern "C" __declspec(dllexport) jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    mc::attach(vm);
    gl_hook::install();
    return JNI_OK;
}

extern "C" __declspec(dllexport) void JNICALL Agent_OnUnload(JavaVM* vm) {
    gl_hook::shutdown();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    return TRUE;
}
