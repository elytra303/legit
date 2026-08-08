#include "gui.h"
#include "font_data.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include <GL/gl.h>

#include "../client.h"
#include "../config.h"
#include "../jvm.h"
#include "../module.h"
#include "../overlay/overlay.h"
#include "../util/log.h"
#include "mc/minecraft.h"

namespace summer {
namespace gui {

namespace {

const ImVec4 kPanelBg(0.13f, 0.15f, 0.18f, 0.96f);
const ImVec4 kPanelDark(0.09f, 0.10f, 0.12f, 1.00f);
const ImVec4 kAccent(0.35f, 0.55f, 0.85f, 1.00f);
const ImVec4 kAccentDark(0.22f, 0.34f, 0.54f, 1.00f);
const ImVec4 kText(0.83f, 0.86f, 0.90f, 1.00f);
const ImVec4 kTextDim(0.55f, 0.58f, 0.63f, 1.00f);

ImFont* g_titleFont = nullptr;
ImFont* g_font = nullptr;
int g_tab = 0;
int* g_captureTarget = nullptr;

const char* kTabs[] = {"COMBAT", "VISUALS", "MOVEMENT", "SETTINGS"};

void UpdateKeyCapture() {
    if (!g_captureTarget) return;
    for (int vk = 1; vk < 256; ++vk) {
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
        if (GetAsyncKeyState(vk) & 0x8000) {
            *g_captureTarget = vk;
            g_captureTarget = nullptr;
            return;
        }
    }
}

void DrawHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + s.x, p.y + 48),
                                IM_COL32(58, 88, 150, 255),
                                IM_COL32(26, 30, 38, 255),
                                IM_COL32(26, 30, 38, 255),
                                IM_COL32(58, 88, 150, 255));
    if (g_titleFont) {
        dl->AddText(g_titleFont, 20.f, ImVec2(p.x + 14, p.y + 10),
                    IM_COL32(245, 248, 252, 255), "SUMMER CLIENT");
        dl->AddText(g_font, 12.f, ImVec2(p.x + 14, p.y + 30),
                    IM_COL32(140, 155, 175, 255), "1.21.x  |  legit edition");
    }
    ImGui::SetCursorPos(ImVec2(8, 52));
}

int DrawTabs() {
    ImGui::BeginChild("tabs", ImVec2(130, ImGui::GetContentRegionAvail().y), false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::Spacing();
    for (int i = 0; i < 4; ++i) {
        bool sel = (g_tab == i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? kAccent : kPanelDark);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sel ? kAccent : kAccentDark);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1, 1, 1, 1) : kText);
        if (ImGui::Button(kTabs[i], ImVec2(-1, 30))) g_tab = i;
        ImGui::PopStyleColor(4);
        ImGui::Spacing();
    }
    ImGui::EndChild();
    return g_tab;
}

void DrawSettingsTab(Client& c) {
    gui::Section("CLIENT");
    ImGui::Spacing();

    bool wm = g_config.GetBool("client.watermark", true);
    if (gui::Checkbox("Watermark", &wm)) g_config.SetBool("client.watermark", wm);
    gui::Help("Shows the client name in the top-left corner");

    ImGui::TextUnformatted("Menu Key");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 92);
    int menuKey = g_config.GetInt("client.menuKey", VK_INSERT);
    if (gui::KeybindButton(&menuKey)) {
        // captured in KeybindButton
    }
    g_config.SetInt("client.menuKey", menuKey);

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save config", ImVec2(140, 26))) c.SaveConfig();
    ImGui::SameLine();
    if (ImGui::Button("Load config", ImVec2(140, 26))) c.LoadConfig();
    ImGui::Spacing();
    if (ImGui::Button("Dump class mappings", ImVec2(180, 26))) {
        JVM::DumpKnownClasses();
        Log("[GUI] dumped class mappings to log");
    }
    gui::Help("Mappings dump goes to %APPDATA%\\SummerClient\\summer.log");

    ImGui::Separator();
    ImGui::Spacing();
    gui::Section("ABOUT");
    gui::Help("Summer Client - legit focused external for Minecraft 1.21.x");
    gui::Help("Injected as a JVMTI agent. Use at your own risk.");
}

void DrawCategory(Client& c, Category cat) {
    gui::Section(CategoryName(cat));
    ImGui::Spacing();
    for (auto* m : c.Modules()) {
        if (m->Cat() != cat) continue;
        bool en = m->Enabled();
        if (gui::Checkbox(m->Name(), &en)) m->Toggle();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        gui::KeybindButton(&m->Key());

        if (m->Enabled() || ImGui::IsItemHovered()) {
            ImGui::Spacing();
            ImGui::Indent(10.f);
            m->DrawSettings();
            ImGui::Unindent(10.f);
            ImGui::Spacing();
            ImGui::Separator();
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------

void ApplyTheme() {
    ImGuiIO& io = ImGui::GetIO();

    // Use the Verdana TTF embedded in font_data.h so glyph coverage
    // (incl. Cyrillic for RU player names) does not depend on the fonts
    // installed on the target machine. The static array outlives ImGui.
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.GlyphRanges = ranges;
    g_font = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 14.f, &cfg);
    if (!g_font) g_font = io.Fonts->AddFontDefault();
    cfg.GlyphRanges = ranges;
    g_titleFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 20.f, &cfg);
    if (!g_titleFont) g_titleFont = g_font;
    io.FontDefault = g_font;

    // Force the atlas build now (normally lazy) so failures surface in the
    // log instead of silently producing a garbage texture.
    bool built = io.Fonts->Build();
    Log("[GUI] fonts: regular=%p title=%p verdana=%u bytes", (void*)g_font,
        (void*)g_titleFont, (unsigned)sizeof(gui::kFontVerdana));
    Log("[GUI] atlas build=%d texture=%dx%d", built ? 1 : 0,
        (int)io.Fonts->TexWidth, (int)io.Fonts->TexHeight);
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    Log("[GUI] GL_MAX_TEXTURE_SIZE=%d", (int)maxTex);

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(6, 4);
    s.ItemSpacing = ImVec2(8, 5);
    s.ItemInnerSpacing = ImVec2(5, 4);
    s.WindowBorderSize = 1.f;
    s.FrameBorderSize = 1.f;
    s.WindowRounding = 2.f;
    s.FrameRounding = 0.f;
    s.ScrollbarSize = 8.f;
    s.ScrollbarRounding = 0.f;

    s.Colors[ImGuiCol_WindowBg] = kPanelBg;
    s.Colors[ImGuiCol_ChildBg] = kPanelDark;
    s.Colors[ImGuiCol_Border] = ImVec4(0.22f, 0.26f, 0.32f, 1.f);
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.22f, 1.f);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.23f, 0.28f, 1.f);
    s.Colors[ImGuiCol_FrameBgActive] = kAccentDark;
    s.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.18f, 0.22f, 1.f);
    s.Colors[ImGuiCol_ButtonHovered] = kAccentDark;
    s.Colors[ImGuiCol_ButtonActive] = kAccent;
    s.Colors[ImGuiCol_SliderGrab] = kAccent;
    s.Colors[ImGuiCol_SliderGrabActive] = kAccent;
    s.Colors[ImGuiCol_Text] = kText;
    s.Colors[ImGuiCol_TextDisabled] = kTextDim;
    s.Colors[ImGuiCol_Header] = kAccentDark;
    s.Colors[ImGuiCol_HeaderHovered] = kAccent;
    s.Colors[ImGuiCol_HeaderActive] = kAccent;
    s.Colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.26f, 0.32f, 1.f);
    s.Colors[ImGuiCol_CheckMark] = ImVec4(1, 1, 1, 1);
}

void DrawMainMenu() {
    UpdateKeyCapture();

    Client& c = Client::Instance();
    ImGui::SetNextWindowSize(ImVec2(760, 470), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(90, 60), ImGuiCond_Once);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("##SummerMenu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    DrawHeader();

    int tab = DrawTabs();
    ImGui::SameLine();
    ImGui::BeginChild("content", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginChild("scroll", ImVec2(0, 0), false);
    switch (tab) {
        case 0:
            DrawCategory(c, Category::Combat);
            break;
        case 1:
            DrawCategory(c, Category::Visual);
            break;
        case 2:
            DrawCategory(c, Category::Movement);
            break;
        default:
            DrawSettingsTab(c);
            break;
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawWatermark() {
    if (!g_config.GetBool("client.watermark", true)) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 p(8, 8);
    dl->AddText(g_font, 16.f, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 160),
                "SUMMER CLIENT");
    dl->AddText(g_font, 16.f, p, IM_COL32(120, 160, 215, 255), "SUMMER CLIENT");
}

ImDrawList* WorldDrawList() { return ImGui::GetBackgroundDrawList(); }

// ---------------------------------------------------------------------------

bool Checkbox(const char* label, bool* v) {
    ImGui::PushID(label);
    bool changed = false;
    ImGui::BeginGroup();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + 14, p.y + 14),
                      *v ? IM_COL32(90, 140, 217, 255) : IM_COL32(42, 46, 55, 255),
                      2.f);
    if (*v) dl->AddText(ImVec2(p.x + 2, p.y), IM_COL32(255, 255, 255, 255), "x");
    ImGui::Dummy(ImVec2(14, 14));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    if (ImGui::IsItemClicked(0)) {
        *v = !*v;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool SliderInt(const char* label, int* v, int mn, int mx) {
    ImGui::TextUnformatted(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 36);
    ImGui::TextColored(kAccent, "%d", *v);
    char id[96];
    snprintf(id, sizeof id, "##sli_%s", label);
    return ImGui::SliderInt(id, v, mn, mx);
}

bool SliderFloat(const char* label, float* v, float mn, float mx) {
    ImGui::TextUnformatted(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 36);
    ImGui::TextColored(kAccent, "%.1f", *v);
    char id[96];
    snprintf(id, sizeof id, "##slf_%s", label);
    return ImGui::SliderFloat(id, v, mn, mx);
}

bool Combo(const char* label, int* idx, const char* const* items, int count) {
    ImGui::TextUnformatted(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 130);
    char id[96];
    snprintf(id, sizeof id, "##cb_%s", label);
    bool changed = false;
    if (ImGui::BeginCombo(id, items[*idx], ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < count; ++i) {
            bool sel = (i == *idx);
            if (ImGui::Selectable(items[i], sel)) {
                *idx = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

const char* KeyName(int vk) {
    static char buf[32];
    switch (vk) {
        case 0: return "NONE";
        case VK_INSERT: return "INS";
        case VK_LBUTTON: return "LMB";
        case VK_RBUTTON: return "RMB";
        case VK_CONTROL: return "CTRL";
        case VK_SHIFT: return "SHIFT";
        case VK_MENU: return "ALT";
        case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER";
        case VK_BACK: return "BACK";
        case VK_TAB: return "TAB";
        case VK_ESCAPE: return "ESC";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        case VK_LEFT: return "LEFT";
        case VK_RIGHT: return "RIGHT";
        case VK_DELETE: return "DEL";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        case VK_PRIOR: return "PGUP";
        case VK_NEXT: return "PGDN";
        default:
            if (vk >= 'A' && vk <= 'Z') {
                buf[0] = (char)vk;
                buf[1] = 0;
                return buf;
            }
            if (vk >= '0' && vk <= '9') {
                buf[0] = (char)vk;
                buf[1] = 0;
                return buf;
            }
            if (vk >= VK_F1 && vk <= VK_F12) {
                snprintf(buf, sizeof buf, "F%d", vk - VK_F1 + 1);
                return buf;
            }
            snprintf(buf, sizeof buf, "VK%d", vk);
            return buf;
    }
}

bool KeybindButton(int* key) {
    bool capture = (g_captureTarget == key);
    char buf[64];
    if (capture)
        snprintf(buf, sizeof buf, "...");
    else
        snprintf(buf, sizeof buf, "[ %s ]", KeyName(*key));
    ImGui::PushID(key);
    bool clicked = ImGui::Button(buf, ImVec2(84, 0));
    ImGui::PopID();
    if (clicked) g_captureTarget = capture ? nullptr : key;
    return capture;
}

bool Keybind(const char* label, int* key) {
    ImGui::TextUnformatted(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 92);
    return KeybindButton(key);
}

void Section(const char* title) {
    ImGui::TextColored(kAccent, "%s", title);
    ImGui::Separator();
}

void Separator() { ImGui::Separator(); }

void Help(const char* text) { ImGui::TextDisabled("%s", text); }

}  // namespace gui
}  // namespace summer
