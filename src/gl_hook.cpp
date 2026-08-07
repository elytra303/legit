#include "gl_hook.h"
#include "config.h"
#include "gui.h"
#include <windows.h>
#include <cstring>
#include <gl3w.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);
typedef GLint(*glGetUniformLocation_t)(GLuint, const GLchar*);
typedef void(*glUniform1f_t)(GLint, GLfloat);
typedef void(*glUniform1fv_t)(GLint, GLsizei, const GLfloat*);

static wglSwapBuffers_t orig_wglSwapBuffers = nullptr;
static glGetUniformLocation_t orig_glGetUniformLocation = nullptr;
static glUniform1f_t orig_glUniform1f = nullptr;
static glUniform1fv_t orig_glUniform1fv = nullptr;

struct FogLocs {
    GLuint program;
    GLint start;
    GLint end;
};

static FogLocs g_fog[64];
static int g_fog_n = 0;
static bool g_gl_ready = false;
static HWND g_hwnd = nullptr;
static WNDPROC g_orig_wndproc = nullptr;

static void check_fog(GLint loc, GLfloat* values, int count) {
    if (!cfg::fog_removed.load() || loc < 0 || count <= 0) return;
    GLint cur = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    for (int i = 0; i < g_fog_n; i++) {
        if (g_fog[i].program == cur && (g_fog[i].start == loc || g_fog[i].end == loc)) {
            for (int k = 0; k < count; k++) values[k] = 1.0e9f;
            return;
        }
    }
}

static GLint WINAPI hk_glGetUniformLocation(GLuint program, const GLchar* name) {
    GLint loc = orig_glGetUniformLocation(program, name);
    if (name && loc >= 0) {
        if (strcmp(name, "fogStart") == 0 || strcmp(name, "fogEnd") == 0) {
            for (int i = 0; i < g_fog_n; i++) {
                if (g_fog[i].program == program) {
                    if (strcmp(name, "fogStart") == 0) g_fog[i].start = loc;
                    else g_fog[i].end = loc;
                    return loc;
                }
            }
            if (g_fog_n < 64) {
                FogLocs& f = g_fog[g_fog_n++];
                f.program = program;
                f.start = f.end = -1;
                if (strcmp(name, "fogStart") == 0) f.start = loc;
                else f.end = loc;
            }
        }
    }
    return loc;
}

static void WINAPI hk_glUniform1f(GLint loc, GLfloat v) {
    GLfloat val = v;
    check_fog(loc, &val, 1);
    orig_glUniform1f(loc, val);
}

static void WINAPI hk_glUniform1fv(GLint loc, GLsizei n, const GLfloat* v) {
    if (n > 0) {
        GLfloat buf[16];
        int c = n < 16 ? (int)n : 16;
        memcpy(buf, v, sizeof(GLfloat) * c);
        check_fog(loc, buf, c);
        orig_glUniform1fv(loc, n, buf);
    } else {
        orig_glUniform1fv(loc, n, v);
    }
}

static LRESULT CALLBACK hk_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) && cfg::g.gui_open)
        return true;
    return CallWindowProc(g_orig_wndproc, hwnd, msg, wparam, lparam);
}

static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
    if (!g_gl_ready) {
        HGLRC ctx = wglGetCurrentContext();
        if (ctx) {
            if (gl3wInit() == 0) {
                g_hwnd = WindowFromDC(hdc);
                if (g_hwnd)
                    g_orig_wndproc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hk_wndproc);
                GLint vp[4] = { 0, 0, 0, 0 };
                glGetIntegerv(GL_VIEWPORT, vp);
                g_gl_ready = gui::init(g_hwnd, vp[2] > 0 ? vp[2] : 1280, vp[3] > 0 ? vp[3] : 720);
            }
        }
    }
    if (g_gl_ready)
        gui::render();
    return orig_wglSwapBuffers(hdc);
}

bool gl_hook::install() {
    if (MH_Initialize() != MH_OK) return false;
    MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers", &hk_wglSwapBuffers, (LPVOID*)&orig_wglSwapBuffers);
    MH_CreateHookApi(L"opengl32.dll", "glGetUniformLocation", &hk_glGetUniformLocation, (LPVOID*)&orig_glGetUniformLocation);
    MH_CreateHookApi(L"opengl32.dll", "glUniform1f", &hk_glUniform1f, (LPVOID*)&orig_glUniform1f);
    MH_CreateHookApi(L"opengl32.dll", "glUniform1fv", &hk_glUniform1fv, (LPVOID*)&orig_glUniform1fv);
    MH_EnableHook(MH_ALL_HOOKS);
    return true;
}

void gl_hook::shutdown() {
    if (g_orig_wndproc && g_hwnd) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_orig_wndproc);
        g_orig_wndproc = nullptr;
    }
    gui::shutdown();
    MH_Uninitialize();
}
