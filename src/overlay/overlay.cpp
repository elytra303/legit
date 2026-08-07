#include "overlay.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include "../client.h"
#include "../config.h"
#include "../gui/gui.h"
#include "../mc/minecraft.h"
#include "../module.h"
#include "../util/log.h"

namespace summer {

Overlay& Overlay::Instance() {
    static Overlay o;
    return o;
}

HWND Overlay::GetGameWindow() {
    if (!window_) return nullptr;
    typedef void* (*Win32Window_t)(void*);
    static Win32Window_t fn = nullptr;
    if (!fn) fn = (Win32Window_t)GlfwProc("glfwGetWin32Window");
    if (!fn) return nullptr;
    return (HWND)fn(window_);
}

void Overlay::Shutdown() {
    if (glReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
        glReady_ = false;
        imguiReady_ = false;
    }
}

static void FeedKeys(ImGuiIO& io) {
    struct KeyMap {
        int vk;
        ImGuiKey key;
    };
    static const KeyMap kMap[] = {
        {'A', ImGuiKey_A}, {'B', ImGuiKey_B}, {'C', ImGuiKey_C},
        {'D', ImGuiKey_D}, {'E', ImGuiKey_E}, {'F', ImGuiKey_F},
        {'G', ImGuiKey_G}, {'H', ImGuiKey_H}, {'I', ImGuiKey_I},
        {'J', ImGuiKey_J}, {'K', ImGuiKey_K}, {'L', ImGuiKey_L},
        {'M', ImGuiKey_M}, {'N', ImGuiKey_N}, {'O', ImGuiKey_O},
        {'P', ImGuiKey_P}, {'Q', ImGuiKey_Q}, {'R', ImGuiKey_R},
        {'S', ImGuiKey_S}, {'T', ImGuiKey_T}, {'U', ImGuiKey_U},
        {'V', ImGuiKey_V}, {'W', ImGuiKey_W}, {'X', ImGuiKey_X},
        {'Y', ImGuiKey_Y}, {'Z', ImGuiKey_Z},
        {'0', ImGuiKey_0}, {'1', ImGuiKey_1}, {'2', ImGuiKey_2},
        {'3', ImGuiKey_3}, {'4', ImGuiKey_4}, {'5', ImGuiKey_5},
        {'6', ImGuiKey_6}, {'7', ImGuiKey_7}, {'8', ImGuiKey_8},
        {'9', ImGuiKey_9},
        {VK_RETURN, ImGuiKey_Enter}, {VK_ESCAPE, ImGuiKey_Escape},
        {VK_BACK, ImGuiKey_Backspace}, {VK_SPACE, ImGuiKey_Space},
        {VK_TAB, ImGuiKey_Tab}, {VK_DELETE, ImGuiKey_Delete},
        {VK_UP, ImGuiKey_UpArrow}, {VK_DOWN, ImGuiKey_DownArrow},
        {VK_LEFT, ImGuiKey_LeftArrow}, {VK_RIGHT, ImGuiKey_RightArrow},
        {VK_HOME, ImGuiKey_Home}, {VK_END, ImGuiKey_End},
        {VK_PRIOR, ImGuiKey_PageUp}, {VK_NEXT, ImGuiKey_PageDown},
        {VK_INSERT, ImGuiKey_Insert},
        {VK_SHIFT, ImGuiKey_LeftShift}, {VK_CONTROL, ImGuiKey_LeftCtrl},
        {VK_MENU, ImGuiKey_LeftAlt},
        {VK_F1, ImGuiKey_F1}, {VK_F2, ImGuiKey_F2}, {VK_F3, ImGuiKey_F3},
        {VK_F4, ImGuiKey_F4}, {VK_F5, ImGuiKey_F5}, {VK_F6, ImGuiKey_F6},
        {VK_F7, ImGuiKey_F7}, {VK_F8, ImGuiKey_F8},
    };
    for (const auto& k : kMap)
        io.AddKeyEvent(k.key, (GetAsyncKeyState(k.vk) & 0x8000) != 0);
}

void Overlay::OnSwap() {
    if (!window_) return;

    if (!glReady_) {
        if (!wglGetCurrentContext()) return;
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplOpenGL3_Init("#version 130");
        glReady_ = true;
    }
    if (!imguiReady_) {
        gui::ApplyTheme();
        imguiReady_ = true;
    }

    // INSERT toggles the menu
    bool insert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insert && !insertPrev_) {
        Client& c = Client::Instance();
        c.SetMenuOpen(!c.MenuOpen());
        c.SaveConfig();
    }
    insertPrev_ = insert ? 1 : 0;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // --- input ---
    ImGuiIO& io = ImGui::GetIO();
    int fbw = 0, fbh = 0;
    typedef void (*FramebufferSize_t)(void*, int*, int*);
    static FramebufferSize_t fbsize = nullptr;
    if (!fbsize) fbsize = (FramebufferSize_t)GlfwProc("glfwGetFramebufferSize");
    if (fbsize) fbsize(window_, &fbw, &fbh);
    if (fbw > 0 && fbh > 0) io.DisplaySize = ImVec2((float)fbw, (float)fbh);

    if (Client::Instance().MenuOpen()) {
        POINT p;
        if (GetCursorPos(&p)) {
            HWND hwnd = GetGameWindow();
            RECT r;
            if (hwnd && GetClientRect(hwnd, &r)) {
                int cw = r.right - r.left, ch = r.bottom - r.top;
                if (cw > 0 && ch > 0) {
                    float mx = (float)(p.x - r.left) * (float)fbw / (float)cw;
                    float my = (float)(p.y - r.top) * (float)fbh / (float)ch;
                    io.AddMousePosEvent(mx, my);
                }
            }
        }
        io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
        int wheel = mc::ReadWheelDelta();
        if (wheel) io.AddMouseWheelEvent(0.f, wheel > 0 ? 1.f : -1.f);
        FeedKeys(io);
    } else {
        io.ClearInputKeys();
    }

    // --- client frame (capture + combat/movement modules) ---
    Client::Instance().OnFrame();

    // --- draw ---
    if (Client::Instance().MenuOpen()) gui::DrawMainMenu();
    if (Client::Instance().IsInGame()) {
        for (auto* m : Client::Instance().Modules()) {
            if (m->Enabled() && m->Cat() == Category::Visual) m->OnRender();
        }
    }
    gui::DrawWatermark();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace summer
