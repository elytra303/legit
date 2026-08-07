#pragma once

#include <windows.h>

namespace summer {

// Resolve a function from the glfw native library loaded by the game.
void* GlfwProc(const char* name);

class Overlay {
public:
    static Overlay& Instance();

    void SetWindowHandle(void* wnd) { window_ = wnd; }
    void OnSwap();
    void Shutdown();

    HWND GetGameWindow();

private:
    void* window_ = nullptr;
    unsigned char insertPrev_ = 0;
    bool glReady_ = false;
    bool imguiReady_ = false;
};

}  // namespace summer
