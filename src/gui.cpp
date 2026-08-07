#include "gui.h"
#include "config.h"
#include "minecraft.h"
#include "modules.h"
#include <windows.h>
#include <cstdio>
#include <gl3w.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

static HWND g_hwnd = nullptr;
static int g_screen_w = 1280;
static int g_screen_h = 720;
static int g_tab = 0;

static const char* vk_name(int vk) {
    switch (vk) {
    case VK_INSERT: return "INS";
    case VK_SHIFT: return "SHIFT";
    case VK_CONTROL: return "CTRL";
    case VK_MENU: return "ALT";
    case VK_SPACE: return "SPACE";
    case VK_TAB: return "TAB";
    case VK_ESCAPE: return "ESC";
    case VK_RETURN: return "ENTER";
    case VK_BACK: return "BACK";
    case VK_DELETE: return "DEL";
    case VK_MBUTTON: return "MB3";
    default:
        if (vk >= 'A' && vk <= 'Z') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
        if (vk >= '0' && vk <= '9') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
        if (vk >= VK_F1 && vk <= VK_F12) { static char b[8]; sprintf(b, "F%d", vk - VK_F1 + 1); return b; }
        return "?";
    }
}

static void mod_row(const char* label, bool* value, int mod_id, const char* desc) {
    ImGui::Checkbox(label, value);
    ImGui::SameLine();
    char bind[64];
    sprintf(bind, "%s##k%d", vk_name(cfg::g.keys[mod_id]), mod_id);
    if (ImGui::Button(bind, ImVec2(48, 0)))
        cfg::g.binding_target = mod_id;
    if (desc) {
        ImGui::SameLine();
        ImGui::TextDisabled(desc);
    }
}

static void hitbox_radio() {
    const char* names[3] = { "OFF", "NORMAL", "LEGIT" };
    for (int i = 0; i < 3; i++) {
        if (i) ImGui::SameLine();
        if (cfg::g.hitbox_mode == i)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.78f, 1.00f));
        if (ImGui::Button(names[i], ImVec2(64, 0)))
            cfg::g.hitbox_mode = i;
        if (cfg::g.hitbox_mode == i)
            ImGui::PopStyleColor();
    }
}

static void tab_combat() {
    ImGui::Text("HITBOXES");
    hitbox_radio();
    if (cfg::g.hitbox_mode == cfg::HITBOX_LEGIT) {
        ImGui::Indent();
        ImGui::SliderFloat("Aim FOV", &cfg::g.hitbox_fov, 1.0f, 30.0f, "%.0f deg");
        ImGui::SliderFloat("Range", &cfg::g.hitbox_range, 2.0f, 6.0f, "%.1f");
        ImGui::Checkbox("Random hit point", &cfg::g.hitbox_random_point);
        ImGui::Checkbox("Draw hitboxes", &cfg::g.hitbox_render);
        ImGui::SliderFloat("Delay", &cfg::g.hitbox_delay_ms, 0.0f, 250.0f, "%.0f ms");
        ImGui::Unindent();
    } else {
        ImGui::Checkbox("Draw hitboxes", &cfg::g.hitbox_render);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("TRIGGERBOT");
    mod_row("Enabled", &cfg::g.triggerbot, cfg::MOD_TRIGGERBOT, nullptr);
    if (cfg::g.triggerbot) {
        ImGui::Indent();
        ImGui::SliderFloat("Range", &cfg::g.trigger_range, 2.0f, 6.0f, "%.1f");
        ImGui::SliderFloat("Delay", &cfg::g.trigger_delay_ms, 50.0f, 400.0f, "%.0f ms");
        ImGui::Unindent();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("AUTOSWAP");
    mod_row("Enabled", &cfg::g.autoswap, cfg::MOD_AUTOSWAP, "swap to best sword/axe on attack");
    if (cfg::g.autoswap) {
        ImGui::Indent();
        ImGui::Checkbox("Swap back", &cfg::g.autoswap_back);
        ImGui::Unindent();
    }
}

static void tab_visual() {
    ImGui::Text("ESP");
    mod_row("Enabled", &cfg::g.esp, cfg::MOD_ESP, nullptr);
    if (cfg::g.esp) {
        ImGui::Indent();
        ImGui::Checkbox("Box", &cfg::g.esp_box);
        ImGui::Checkbox("Health", &cfg::g.esp_health);
        ImGui::Checkbox("Distance", &cfg::g.esp_distance);
        ImGui::Checkbox("Tracers", &cfg::g.esp_tracers);
        ImGui::Unindent();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("REMOVALS");
    mod_row("Enabled", &cfg::g.rem_fire, cfg::MOD_REMOVALS, nullptr);
    if (cfg::g.rem_fire || cfg::g.rem_fog || cfg::g.rem_nausea) {
        ImGui::Indent();
        ImGui::Checkbox("Fire overlay", &cfg::g.rem_fire);
        ImGui::Checkbox("Fog", &cfg::g.rem_fog);
        ImGui::Checkbox("Nausea", &cfg::g.rem_nausea);
        ImGui::Unindent();
    }
}

static void tab_movement() {
    ImGui::Text("SPRINT");
    mod_row("Always sprint", &cfg::g.sprint, cfg::MOD_SPRINT, "while moving forward");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("SCREEN WALK");
    mod_row("Enabled", &cfg::g.screenwalk, cfg::MOD_SCREENWALK, "walk with inventory open");
}

static void tab_hud() {
    ImGui::Text("HUD");
    mod_row("Enabled", &cfg::g.hud, cfg::MOD_HUD, nullptr);
    if (cfg::g.hud) {
        ImGui::Indent();
        ImGui::Checkbox("FPS", &cfg::g.hud_fps);
        ImGui::Checkbox("Coordinates", &cfg::g.hud_coords);
        ImGui::Checkbox("Direction", &cfg::g.hud_direction);
        ImGui::Checkbox("Modules list", &cfg::g.hud_modules);
        ImGui::Unindent();
    }
}

static void draw_menu() {
    ImGui::SetNextWindowSize(ImVec2(620, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Summer Client", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.18f, 0.52f, 0.78f, 1.00f), "SUMMER CLIENT");
    ImGui::TextDisabled("v1.0.0 - minecraft 1.21.11");
    ImGui::Spacing();

    ImGui::BeginChild("sidebar", ImVec2(140, 280), true);
    const char* tabs[4] = { "COMBAT", "VISUAL", "MOVEMENT", "HUD" };
    for (int i = 0; i < 4; i++) {
        if (ImGui::Selectable(tabs[i], g_tab == i))
            g_tab = i;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("content", ImVec2(0, 280), false);
    ImGui::BeginChild("scroll", ImVec2(0, 0), false);
    switch (g_tab) {
    case 0: tab_combat(); break;
    case 1: tab_visual(); break;
    case 2: tab_movement(); break;
    case 3: tab_hud(); break;
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    char hint[128];
    if (cfg::g.binding_target >= 0)
        sprintf(hint, "press a key to bind...");
    else
        sprintf(hint, "menu: %s  |  click a bind to change it", vk_name(cfg::g.menu_key));
    ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.00f), hint);
    ImGui::SameLine(0, 24);
    if (ImGui::Button("EXIT", ImVec2(64, 24))) {
        JNIEnv* e = mc::env();
        if (e && mc::B.m_stop) {
            jobject c = mc::mc_client(e);
            if (c) {
                e->CallVoidMethod(c, mc::B.m_stop);
                e->DeleteLocalRef(c);
            }
        }
    }
    ImGui::End();
}

bool gui::init(HWND hwnd, int screen_w, int screen_h) {
    g_hwnd = hwnd;
    g_screen_w = screen_w;
    g_screen_h = screen_h;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 0.0f;
    s.ChildRounding = 0.0f;
    s.PopupRounding = 0.0f;
    s.GrabRounding = 0.0f;
    s.TabRounding = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(8, 4);
    s.ItemSpacing = ImVec2(6, 5);
    s.ItemInnerSpacing = ImVec2(6, 4);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.96f);
    c[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.25f, 0.32f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.13f, 0.18f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.18f, 0.52f, 0.78f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.18f, 0.52f, 0.78f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.63f, 0.88f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.13f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.52f, 0.78f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.11f, 0.40f, 0.62f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.52f, 0.78f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.15f, 0.44f, 0.66f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.11f, 0.36f, 0.55f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.52f, 0.78f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.63f, 0.88f, 1.00f);

    return ImGui_ImplWin32_Init(hwnd) && ImGui_ImplOpenGL3_Init("#version 330");
}

void gui::render() {
    JNIEnv* e = mc::env();
    if (e) e->PushLocalFrame(256);
    mc::init(e);

    modules::poll_keys();

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    modules::tick();

    if (cfg::g.gui_open)
        draw_menu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glUseProgram(0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (e) e->PopLocalFrame(nullptr);
}

void gui::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
