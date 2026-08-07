// Summer Client - JVMTI agent entry point.
//
// Loaded with -agentpath:path\summer_client.dll. Resolves the Minecraft
// singleton through JNI, registers all modules and installs a MinHook detour
// on glfwSwapBuffers (in the glfw native library the game already loaded) so
// the overlay renders and the game loop is driven once per frame on the
// render thread.

#include <windows.h>

#include <jni.h>
#include <jvmti.h>

#include <thread>

#include <MinHook.h>

#include "overlay/overlay.h"
#include "summer/client.h"
#include "summer/jvm.h"
#include "summer/registry.h"
#include "util/log.h"

namespace summer {

namespace {

typedef void (*SwapBuffersFn)(void* window);
SwapBuffersFn origSwapBuffers = nullptr;
void* swapBuffersTarget = nullptr;

HMODULE GlfwModule() {
    HMODULE m = GetModuleHandleA("glfw3.dll");
    if (!m) m = GetModuleHandleA("glfw.dll");
    return m;
}

void* SwapBuffersAddr() {
    HMODULE m = GlfwModule();
    if (!m) return nullptr;
    return (void*)GetProcAddress(m, "glfwSwapBuffers");
}

void HookSwapBuffers(void* window) {
    Overlay::Instance().OnSwap();
    if (origSwapBuffers) origSwapBuffers(window);
}

void TryInstall() {
    if (origSwapBuffers) return;
    void* target = SwapBuffersAddr();
    if (!target) return;
    if (MH_Initialize() != MH_OK) return;
    if (MH_CreateHook(target, reinterpret_cast<LPVOID>(&HookSwapBuffers),
                      (void**)&origSwapBuffers) != MH_OK)
        return;
    if (MH_EnableHook(target) != MH_OK) {
        origSwapBuffers = nullptr;
        return;
    }
    swapBuffersTarget = target;
    Log("[Summer] glfwSwapBuffers hooked");
}

void WatchThread() {
    for (;;) {
        TryInstall();
        if (origSwapBuffers) return;
        Sleep(400);
    }
}

}  // namespace

// Resolve any glfw native function lazily (used by the overlay).
void* GlfwProc(const char* name) {
    HMODULE m = GlfwModule();
    if (!m) return nullptr;
    return (void*)GetProcAddress(m, name);
}

}  // namespace summer

extern "C" __declspec(dllexport) jint JNICALL Agent_OnLoad(JavaVM* vm, char* options,
                                                           void* reserved) {
    using namespace summer;
    LogInit();
    Log("[Summer] agent loaded (options: %s)", options ? options : "");

    jvmtiEnv* jvmti = nullptr;
    jint r = vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);
    if (r != JNI_OK || !jvmti)
        LogWarn("[Summer] JVMTI env unavailable (%d), some tools will not work",
                (int)r);
    JVM::Init(vm, jvmti);

    Client& c = Client::Instance();
    RegisterAllModules(c);
    c.Initialize();

    std::thread(WatchThread).detach();
    return JNI_OK;
}

extern "C" __declspec(dllexport) void JNICALL Agent_OnUnload(JavaVM* vm) {
    using namespace summer;
    Client::Instance().Shutdown();
    if (swapBuffersTarget) {
        MH_DisableHook(swapBuffersTarget);
        MH_Uninitialize();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    return TRUE;
}
