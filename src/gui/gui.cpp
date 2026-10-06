#include "gui.h"
#include "font_data.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cfloat>

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

const ImVec4 kWindowBg(0.055f, 0.063f, 0.078f, 0.98f);
const ImVec4 kSideBg(0.071f, 0.078f, 0.094f, 1.00f);
const ImVec4 kHeadBg(0.082f, 0.090f, 0.106f, 1.00f);
const ImVec4 kCardBg(0.086f, 0.094f, 0.114f, 1.00f);
const ImVec4 kStroke(0.149f, 0.165f, 0.196f, 1.00f);
const ImVec4 kAccent(0.376f, 0.541f, 0.984f, 1.00f);
const ImVec4 kText(0.910f, 0.925f, 0.953f, 1.00f);
const ImVec4 kTextDim(0.545f, 0.584f, 0.655f, 1.00f);

const ImU32 kUSide = IM_COL32(18, 20, 24, 255);
const ImU32 kUHead = IM_COL32(21, 23, 27, 255);
const ImU32 kUCard = IM_COL32(22, 24, 29, 255);
const ImU32 kUCardOn = IM_COL32(24, 30, 45, 255);
const ImU32 kUCardHov = IM_COL32(27, 30, 38, 255);
const ImU32 kUStroke = IM_COL32(38, 42, 51, 255);
const ImU32 kUAccent = IM_COL32(96, 138, 250, 255);
const ImU32 kUText = IM_COL32(232, 236, 243, 255);
const ImU32 kUDim = IM_COL32(139, 149, 167, 255);
const ImU32 kUMuted = IM_COL32(96, 104, 120, 255);

ImFont* g_titleFont = nullptr;
ImFont* g_font = nullptr;
ImFont* g_smallFont = nullptr;
ImFont* g_headFont = nullptr;
int g_tab = 0;
int* g_captureTarget = nullptr;
char g_search[64] = {0};

const char* kTabs[] = {"COMBAT", "VISUALS", "MOVEMENT", "SETTINGS"};

const float kHeadH = 64.f;
const float kSideW = 176.f;

char LowerC(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

bool MatchSearch(const char* name) {
    if (!g_search[0]) return true;
    for (const char* p = name; *p; ++p) {
        const char* a = p;
        const char* b = g_search;
        while (*a && *b && LowerC(*a) == LowerC(*b)) {
            ++a;
            ++b;
        }
        if (!*b) return true;
    }
    return false;
}

void UpdateKeyCapture() {
    if (!g_captureTarget) return;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        g_captureTarget = nullptr;
        return;
    }
    for (int vk = 1; vk < 256; ++vk) {
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
        if (GetAsyncKeyState(vk) & 0x8000) {
            *g_captureTarget = vk;
            g_captureTarget = nullptr;
            return;
        }
    }
}

void DrawMenuShadow(const ImVec2& pos, const ImVec2& size) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 5; i >= 1; --i) {
        float o = (float)i * 2.5f;
        dl->AddRectFilled(ImVec2(pos.x - o, pos.y - o + o * 0.6f),
                          ImVec2(pos.x + size.x + o, pos.y + size.y + o),
                          IM_COL32(0, 0, 0, 6 + i * 5), 14.f + o);
    }
}

void DrawHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + kHeadH), kUHead, 12.f,
                      ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);
    dl->AddRectFilled(ImVec2(p.x + 18.f, p.y + 16.f), ImVec2(p.x + 22.f, p.y + 48.f),
                      kUAccent, 2.f);
    if (g_titleFont) {
        const char* a = "SUMMER";
        const char* b = "CLIENT";
        dl->AddText(g_titleFont, 21.f, ImVec2(p.x + 34.f, p.y + 13.f), kUText, a);
        float aw = g_titleFont->CalcTextSizeA(21.f, FLT_MAX, 0.f, a).x;
        dl->AddText(g_titleFont, 21.f, ImVec2(p.x + 34.f + aw + 8.f, p.y + 13.f),
                    kUAccent, b);
        if (g_smallFont)
            dl->AddText(g_smallFont, 12.5f, ImVec2(p.x + 35.f, p.y + 40.f), kUMuted,
                        "minecraft 1.21.x   legit edition");
    }
    if (g_smallFont) {
        char hint[40];
        snprintf(hint, sizeof hint, "%s to close",
                 KeyName(g_config.GetInt("client.menuKey", VK_INSERT)));
        float hw = g_smallFont->CalcTextSizeA(12.5f, FLT_MAX, 0.f, hint).x;
        ImVec2 hp(p.x + s.x - hw - 34.f, p.y + 21.f);
        dl->AddRectFilled(hp, ImVec2(hp.x + hw + 20.f, hp.y + 22.f),
                          IM_COL32(255, 255, 255, 12), 6.f);
        dl->AddText(g_smallFont, 12.5f, ImVec2(hp.x + 10.f, hp.y + 4.f), kUDim, hint);
    }
    dl->AddRectFilled(ImVec2(p.x, p.y + kHeadH - 1.f), ImVec2(p.x + s.x, p.y + kHeadH),
                      kUStroke);
}

int DrawTabs() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    dl->AddRectFilled(ImVec2(p.x, p.y + kHeadH), ImVec2(p.x + kSideW, p.y + s.y),
                      kUSide, 0.f, ImDrawFlags_RoundCornersBottomLeft);
    dl->AddLine(ImVec2(p.x + kSideW, p.y + kHeadH), ImVec2(p.x + kSideW, p.y + s.y),
                kUStroke);

    float x = p.x + 14.f;
    float w = kSideW - 28.f;
    float h = 40.f;
    float y = p.y + kHeadH + 20.f;
    for (int i = 0; i < 4; ++i) {
        bool sel = (g_tab == i);
        bool hov = ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + w, y + h));
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID(i);
        if (sel)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                              IM_COL32(96, 138, 250, 34), 8.f);
        else if (hov)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                              IM_COL32(255, 255, 255, 10), 8.f);
        if (sel)
            dl->AddRectFilled(ImVec2(x + 1.f, y + 9.f), ImVec2(x + 4.f, y + h - 9.f),
                              kUAccent, 1.5f);
        if (g_font)
            dl->AddText(g_font, 15.f, ImVec2(x + 16.f, y + (h - 15.f) * 0.5f),
                        sel ? kUAccent : (hov ? kUText : kUDim), kTabs[i]);
        ImGui::InvisibleButton("##tab", ImVec2(w, h));
        if (ImGui::IsItemClicked(0) && g_tab != i) {
            g_tab = i;
            g_search[0] = 0;
        }
        ImGui::PopID();
        y += h + 6.f;
    }
    if (g_smallFont)
        dl->AddText(g_smallFont, 12.5f, ImVec2(p.x + 16.f, p.y + s.y - 26.f), kUMuted,
                    "summer client  v1.0.0");
    return g_tab;
}

void CardBegin(const char* id, float h) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleColor(ImGuiCol_Border, kStroke);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f));
    ImGui::BeginChild("##card", ImVec2(0, h), true, ImGuiWindowFlags_NoScrollbar);
}

void CardEnd() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

bool DrawSwitch(const char* id, bool v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = 38.f, h = 20.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg;
    if (v)
        bg = kUAccent;
    else if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + h)))
        bg = IM_COL32(56, 61, 72, 255);
    else
        bg = IM_COL32(44, 48, 58, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
    float kx = v ? (p.x + w - 10.f) : (p.x + 10.f);
    dl->AddCircleFilled(ImVec2(kx, p.y + h * 0.5f), 7.f, IM_COL32(245, 247, 251, 255),
                        12);
    ImGui::InvisibleButton(id, ImVec2(w, h));
    if (ImGui::IsItemClicked(0)) v = !v;
    return v;
}

bool FancyButton(const char* label, const ImVec2& size, bool accent) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + size.x, p.y + size.y));
    ImU32 bg;
    if (accent)
        bg = hov ? IM_COL32(118, 156, 252, 255) : kUAccent;
    else
        bg = hov ? IM_COL32(36, 40, 49, 255) : IM_COL32(27, 30, 38, 255);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 6.f);
    if (!accent)
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), kUStroke, 6.f, 0, 1.f);
    if (g_font) {
        ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(g_font, 15.f,
                    ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
                    accent ? IM_COL32(255, 255, 255, 255) : kUText, label);
    }
    ImGui::InvisibleButton(label, size);
    return ImGui::IsItemClicked(0);
}

void RowLine(float x, float w, float y) {
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x, y), ImVec2(x + w, y),
                                        IM_COL32(34, 38, 47, 255));
}

bool RowSwitch(const char* label, const char* desc, bool* v, bool divider) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = desc ? 46.f : 34.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(g_font, 15.f, ImVec2(p.x, p.y + (desc ? 0.f : (h - 15.f) * 0.5f)),
                kUText, label);
    if (desc && g_smallFont)
        dl->AddText(g_smallFont, 12.5f, ImVec2(p.x, p.y + 21.f), kUDim, desc);
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - 38.f, p.y + (h - 20.f) * 0.5f));
    bool nv = DrawSwitch("##sw", *v);
    bool changed = (nv != *v);
    *v = nv;
    if (divider) RowLine(p.x, w, p.y + h - 1.f);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    return changed;
}

bool RowKey(const char* label, const char* desc, int* key, bool divider) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = desc ? 46.f : 34.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(g_font, 15.f, ImVec2(p.x, p.y + (desc ? 0.f : (h - 15.f) * 0.5f)),
                kUText, label);
    if (desc && g_smallFont)
        dl->AddText(g_smallFont, 12.5f, ImVec2(p.x, p.y + 21.f), kUDim, desc);
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - 76.f, p.y + (h - 22.f) * 0.5f));
    gui::KeybindButton(key);
    if (divider) RowLine(p.x, w, p.y + h - 1.f);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    return false;
}

void DrawSettingsTab(Client& c) {
    bool wm = g_config.GetBool("client.watermark", true);
    int menuKey = g_config.GetInt("client.menuKey", VK_INSERT);

    gui::Section("CLIENT");
    ImGui::Spacing();
    CardBegin("client", 116.f);
    if (RowSwitch("Watermark", "shows the client name in the top-left corner", &wm,
                  true))
        g_config.SetBool("client.watermark", wm);
    RowKey("Menu key", "opens and closes the menu", &menuKey, false);
    g_config.SetInt("client.menuKey", menuKey);
    CardEnd();

    ImGui::Spacing();
    ImGui::Spacing();
    gui::Section("CONFIG");
    ImGui::Spacing();
    CardBegin("cfg", 56.f);
    if (FancyButton("save config", ImVec2(140.f, 32.f), true)) c.SaveConfig();
    ImGui::SameLine();
    if (FancyButton("load config", ImVec2(140.f, 32.f), false)) c.LoadConfig();
    CardEnd();

    ImGui::Spacing();
    ImGui::Spacing();
    gui::Section("DIAGNOSTICS");
    ImGui::Spacing();
    CardBegin("diag", 84.f);
    if (FancyButton("dump class mappings", ImVec2(180.f, 32.f), false)) {
        JVM::DumpKnownClasses();
        Log("[GUI] dumped class mappings to log");
    }
    ImGui::Dummy(ImVec2(0, 6.f));
    gui::Help("goes to %APPDATA%\\SummerClient\\summer.log");
    CardEnd();

    ImGui::Spacing();
    ImGui::Spacing();
    gui::Section("ABOUT");
    ImGui::Spacing();
    CardBegin("about", 66.f);
    gui::Help("summer client - legit focused external for minecraft 1.21.x");
    ImGui::Dummy(ImVec2(0, 4.f));
    gui::Help("injected as a jvmti agent. use at your own risk.");
    CardEnd();
}

void DrawModuleCard(Module* m, float width) {
    ImGui::PushID(m->Name());
    const float h = 58.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 r2(p.x + width, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool on = m->Enabled();
    bool hov = ImGui::IsMouseHoveringRect(p, r2);

    dl->AddRectFilled(p, r2, on ? kUCardOn : (hov ? kUCardHov : kUCard), 8.f);
    dl->AddRect(p, r2, on ? IM_COL32(96, 138, 250, 150) : kUStroke, 8.f, 0, 1.f);
    if (on)
        dl->AddRectFilled(ImVec2(p.x + 2.f, p.y + 13.f), ImVec2(p.x + 5.f, p.y + h - 13.f),
                          kUAccent, 1.5f);
    if (g_font)
        dl->AddText(g_font, 15.f, ImVec2(p.x + 16.f, p.y + 11.f), kUText, m->Name());
    if (m->Desc() && m->Desc()[0] && g_smallFont)
        dl->AddText(g_smallFont, 12.5f, ImVec2(p.x + 16.f, p.y + 32.f), kUDim,
                    m->Desc());

    float sx = r2.x - 16.f - 38.f;
    ImGui::SetCursorScreenPos(ImVec2(sx, p.y + (h - 20.f) * 0.5f));
    bool nv = DrawSwitch("##sw", on);
    if (nv != on) m->Toggle();

    ImGui::SetCursorScreenPos(ImVec2(sx - 12.f - 76.f, p.y + (h - 22.f) * 0.5f));
    gui::KeybindButton(&m->Key());

    ImGui::SetCursorScreenPos(p);
    ImGui::InvisibleButton("##card", ImVec2(width, h));
    if (ImGui::IsItemClicked(0)) m->Toggle();

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 10.f));
    ImGui::PopID();
}

void DrawEmptyState() {
    float w = ImGui::GetContentRegionAvail().x;
    float x = ImGui::GetCursorScreenPos().x;
    float y = ImGui::GetCursorScreenPos().y + 52.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const char* t1 = "nothing here yet";
    const char* t2 = "modules for this section are not in the build right now";
    if (g_font) {
        ImVec2 s1 = ImGui::CalcTextSize(t1);
        dl->AddText(g_font, 15.f, ImVec2(x + (w - s1.x) * 0.5f, y), kUDim, t1);
    }
    if (g_smallFont) {
        ImVec2 s2 = g_smallFont->CalcTextSizeA(12.5f, FLT_MAX, 0.f, t2);
        dl->AddText(g_smallFont, 12.5f, ImVec2(x + (w - s2.x) * 0.5f, y + 26.f),
                    kUMuted, t2);
    }
    ImGui::Dummy(ImVec2(0, 110.f));
}

void DrawCategory(Client& c, Category cat) {
    float width = ImGui::GetContentRegionAvail().x;
    int shown = 0;
    for (auto* m : c.Modules()) {
        if (m->Cat() != cat) continue;
        if (!MatchSearch(m->Name())) continue;
        DrawModuleCard(m, width);
        ++shown;
        if (m->Enabled()) {
            ImGui::Indent(16.f);
            m->DrawSettings();
            ImGui::Unindent(16.f);
            ImGui::Spacing();
        }
    }
    if (!shown) DrawEmptyState();
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
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 15.f, &cfg);
    if (!g_font) g_font = io.Fonts->AddFontDefault();
    cfg.GlyphRanges = ranges;
    g_smallFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 12.5f, &cfg);
    if (!g_smallFont) g_smallFont = g_font;
    cfg.GlyphRanges = ranges;
    g_headFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 17.f, &cfg);
    if (!g_headFont) g_headFont = g_font;
    cfg.GlyphRanges = ranges;
    g_titleFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)sizeof(gui::kFontVerdana), 21.f, &cfg);
    if (!g_titleFont) g_titleFont = g_font;
    io.FontDefault = g_font;

    // Force the atlas build now (normally lazy) so failures surface in the
    // log instead of silently producing a garbage texture.
    bool built = io.Fonts->Build();
    int tw = 0, th = 0;
    unsigned char* tpixels = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&tpixels, &tw, &th);
    Log("[GUI] fonts: regular=%p title=%p verdana=%u bytes", (void*)g_font,
        (void*)g_titleFont, (unsigned)sizeof(gui::kFontVerdana));
    Log("[GUI] atlas build=%d texture=%dx%d", built ? 1 : 0, tw, th);
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    Log("[GUI] GL_MAX_TEXTURE_SIZE=%d", (int)maxTex);

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(10, 10);
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(10, 7);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 1.f;
    s.ChildBorderSize = 1.f;
    s.FrameBorderSize = 0.f;
    s.WindowRounding = 12.f;
    s.ChildRounding = 8.f;
    s.FrameRounding = 6.f;
    s.GrabRounding = 6.f;
    s.ScrollbarSize = 9.f;
    s.ScrollbarRounding = 6.f;
    s.PopupRounding = 8.f;

    s.Colors[ImGuiCol_WindowBg] = kWindowBg;
    s.Colors[ImGuiCol_ChildBg] = kCardBg;
    s.Colors[ImGuiCol_PopupBg] = kSideBg;
    s.Colors[ImGuiCol_Border] = kStroke;
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.102f, 0.114f, 0.137f, 1.f);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.137f, 0.153f, 0.184f, 1.f);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.165f, 0.192f, 0.243f, 1.f);
    s.Colors[ImGuiCol_Button] = ImVec4(0.118f, 0.133f, 0.165f, 1.f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.165f, 0.192f, 0.243f, 1.f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.376f, 0.541f, 0.984f, 0.45f);
    s.Colors[ImGuiCol_SliderGrab] = kAccent;
    s.Colors[ImGuiCol_SliderGrabActive] = kAccent;
    s.Colors[ImGuiCol_Text] = kText;
    s.Colors[ImGuiCol_TextDisabled] = kTextDim;
    s.Colors[ImGuiCol_Header] = ImVec4(0.376f, 0.541f, 0.984f, 0.22f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.376f, 0.541f, 0.984f, 0.35f);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(0.376f, 0.541f, 0.984f, 0.50f);
    s.Colors[ImGuiCol_Separator] = kStroke;
    s.Colors[ImGuiCol_CheckMark] = kAccent;
    s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.063f, 0.078f, 0.6f);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.22f, 0.27f, 1.f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.29f, 0.35f, 1.f);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.376f, 0.541f, 0.984f, 0.8f);
    s.Colors[ImGuiCol_NavCursor] = kAccent;
}

void DrawMainMenu() {
    UpdateKeyCapture();

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x < 320.f || io.DisplaySize.y < 240.f) return;

    Client& c = Client::Instance();
    const ImVec2 size(800.f, 520.f);
    ImVec2 pos((io.DisplaySize.x - size.x) * 0.5f,
               (io.DisplaySize.y - size.y) * 0.5f);
    if (pos.x < 8.f) pos.x = 8.f;
    if (pos.y < 8.f) pos.y = 8.f;

    DrawMenuShadow(pos, size);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##SummerMenu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    DrawHeader();
    int tab = DrawTabs();

    float cx = pos.x + kSideW + 20.f;
    float cw = size.x - kSideW - 40.f;
    float cy = pos.y + kHeadH + 16.f;

    if (g_headFont)
        ImGui::GetWindowDrawList()->AddText(g_headFont, 17.f, ImVec2(cx, cy + 6.f),
                                            kUText, kTabs[tab]);

    if (tab < 3) {
        float sw = 210.f;
        ImGui::SetCursorScreenPos(ImVec2(cx + cw - sw, cy));
        ImGui::SetNextItemWidth(sw);
        ImGui::InputTextWithHint("##search", "search...", g_search, sizeof(g_search));
    }

    float by = cy + 40.f;
    float bh = pos.y + size.y - 16.f - by;
    ImGui::SetCursorScreenPos(ImVec2(cx, by));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##body", ImVec2(cw, bh), false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.f, 10.f));
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
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawWatermark() {
    if (!g_config.GetBool("client.watermark", true)) return;
    if (!g_font || !g_smallFont) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* name = "SUMMER CLIENT";
    char fps[24];
    snprintf(fps, sizeof fps, "%d fps", (int)(ImGui::GetIO().Framerate + 0.5f));
    ImVec2 ts = g_font->CalcTextSizeA(14.f, FLT_MAX, 0.f, name);
    ImVec2 fs = g_smallFont->CalcTextSizeA(12.f, FLT_MAX, 0.f, fps);
    float h = 30.f;
    float w = 26.f + ts.x + 12.f + fs.x + 16.f;
    ImVec2 a(10.f, 10.f), b(10.f + w, 10.f + h);
    dl->AddRectFilled(a, b, IM_COL32(14, 16, 20, 225), 8.f);
    dl->AddRect(a, b, IM_COL32(96, 138, 250, 90), 8.f, 0, 1.f);
    dl->AddCircleFilled(ImVec2(a.x + 15.f, a.y + h * 0.5f), 3.5f, kUAccent);
    dl->AddText(g_font, 14.f, ImVec2(a.x + 26.f, a.y + (h - 14.f) * 0.5f), kUText, name);
    dl->AddText(g_smallFont, 12.f,
                ImVec2(a.x + 26.f + ts.x + 12.f, a.y + (h - 12.f) * 0.5f), kUMuted,
                fps);
}

ImDrawList* WorldDrawList() { return ImGui::GetBackgroundDrawList(); }

// ---------------------------------------------------------------------------

bool Checkbox(const char* label, bool* v) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 30.f;
    if (g_font)
        ImGui::GetWindowDrawList()->AddText(g_font, 15.f,
                                            ImVec2(p.x, p.y + (h - 15.f) * 0.5f),
                                            kUText, label);
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - 38.f, p.y + (h - 20.f) * 0.5f));
    bool nv = DrawSwitch("##sw", *v);
    bool changed = (nv != *v);
    *v = nv;
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
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
        snprintf(buf, sizeof buf, "press a key");
    else
        snprintf(buf, sizeof buf, "%s", KeyName(*key));
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = 76.f, h = 22.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + h));
    ImU32 bg;
    if (capture)
        bg = kUAccent;
    else if (hov)
        bg = IM_COL32(38, 42, 52, 255);
    else
        bg = IM_COL32(27, 30, 38, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, 6.f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), capture ? kUAccent : kUStroke, 6.f, 0, 1.f);
    if (g_smallFont) {
        ImVec2 ts = g_smallFont->CalcTextSizeA(12.5f, FLT_MAX, 0.f, buf);
        dl->AddText(g_smallFont, 12.5f,
                    ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - 12.5f) * 0.5f),
                    capture ? IM_COL32(255, 255, 255, 255) : (hov ? kUText : kUDim),
                    buf);
    }
    ImGui::PushID(key);
    ImGui::InvisibleButton("##key", ImVec2(w, h));
    ImGui::PopID();
    bool clicked = ImGui::IsItemClicked(0);
    if (clicked) g_captureTarget = capture ? nullptr : key;
    return capture;
}

bool Keybind(const char* label, int* key) {
    ImGui::TextUnformatted(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 76);
    return KeybindButton(key);
}

void Section(const char* title) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y + 3.f), ImVec2(p.x + 3.f, p.y + 13.f), kUAccent,
                      1.5f);
    if (g_smallFont)
        dl->AddText(g_smallFont, 12.5f, ImVec2(p.x + 11.f, p.y + 1.f), kUDim, title);
    ImGui::Dummy(ImVec2(0, 17.f));
}

void Separator() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + w, p.y),
                                        IM_COL32(34, 38, 47, 255));
    ImGui::Dummy(ImVec2(0, 8.f));
}

void Help(const char* text) {
    if (!g_smallFont) {
        ImGui::TextDisabled("%s", text);
        return;
    }
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    if (w < 40.f) w = 40.f;
    ImVec2 ts = g_smallFont->CalcTextSizeA(12.5f, FLT_MAX, w, text);
    ImGui::GetWindowDrawList()->AddText(g_smallFont, 12.5f, p, kUDim, text, NULL, w);
    ImGui::Dummy(ImVec2(0, ts.y + 3.f));
}

}  // namespace gui
}  // namespace summer
