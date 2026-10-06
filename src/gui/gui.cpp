#include "gui.h"
#include "font_data.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <unordered_map>

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

// Baked font sizes. ImGui 1.92 rasterizes text at the exact size it is drawn
// at, so draw every string at its font's LegacySize to stay crisp.
const float kSizeSmall = 13.f;
const float kSizeBase = 15.f;
const float kSizeHead = 17.f;
const float kSizeTitle = 21.f;

ImFont* g_titleFont = nullptr;
ImFont* g_font = nullptr;
ImFont* g_smallFont = nullptr;
ImFont* g_headFont = nullptr;
int g_tab = 0;
int* g_captureTarget = nullptr;
char g_search[64] = {0};

float g_menuAnim = 0.f;  // 0..1 menu open progress

const char* kTabs[] = {"COMBAT", "VISUALS", "MOVEMENT", "SETTINGS"};

const float kHeadH = 64.f;
const float kSideW = 176.f;

// Exponential smoothing toward `target`. `speed` is the response rate
// (higher = snappier). Values snap to target once close enough.
float AnimF(ImU32 id, float target, float speed) {
    static std::unordered_map<ImU32, float> vals;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.f || dt > 0.5f) dt = 1.f / 60.f;
    float& v = vals[id];
    float k = 1.f - std::exp(-speed * dt);
    v += (target - v) * k;
    if (std::fabs(v - target) < 0.002f) v = target;
    return v;
}

ImU32 LerpCol(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    int ar = (int)((a >> 0) & 0xFF), ag = (int)((a >> 8) & 0xFF),
        ab = (int)((a >> 16) & 0xFF), aa = (int)((a >> 24) & 0xFF);
    int br = (int)((b >> 0) & 0xFF), bg = (int)((b >> 8) & 0xFF),
        bb = (int)((b >> 16) & 0xFF), ba = (int)((b >> 24) & 0xFF);
    int r = (int)(ar + (br - ar) * t);
    int g = (int)(ag + (bg - ag) * t);
    int bl = (int)(ab + (bb - ab) * t);
    int al = (int)(aa + (ba - aa) * t);
    return IM_COL32(r, g, bl, al);
}

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

void DrawMenuShadow(const ImVec2& pos, const ImVec2& size, float alpha) {
    if (alpha <= 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 5; i >= 1; --i) {
        float o = (float)i * 2.5f;
        dl->AddRectFilled(ImVec2(pos.x - o, pos.y - o + o * 0.6f),
                          ImVec2(pos.x + size.x + o, pos.y + size.y + o),
                          IM_COL32(0, 0, 0, (int)((6 + i * 5) * alpha)), 14.f + o);
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
        dl->AddText(g_titleFont, kSizeTitle, ImVec2(p.x + 34.f, p.y + 13.f), kUText, a);
        float aw = g_titleFont->CalcTextSizeA(kSizeTitle, FLT_MAX, 0.f, a).x;
        dl->AddText(g_titleFont, kSizeTitle, ImVec2(p.x + 34.f + aw + 8.f, p.y + 13.f),
                    kUAccent, b);
        if (g_smallFont)
            dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x + 35.f, p.y + 40.f), kUMuted,
                        "minecraft 1.21.x   legit edition");
    }
    if (g_smallFont) {
        char hint[40];
        snprintf(hint, sizeof hint, "%s to close",
                 KeyName(g_config.GetInt("client.menuKey", VK_INSERT)));
        float hw = g_smallFont->CalcTextSizeA(kSizeSmall, FLT_MAX, 0.f, hint).x;
        ImVec2 hp(p.x + s.x - hw - 34.f, p.y + 21.f);
        dl->AddRectFilled(hp, ImVec2(hp.x + hw + 20.f, hp.y + 22.f),
                          IM_COL32(255, 255, 255, 12), 6.f);
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(hp.x + 10.f, hp.y + 4.f), kUDim, hint);
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
    float y0 = p.y + kHeadH + 20.f;
    float y = y0;

    // sliding pill that follows the selected tab
    static float s_pillY = -1.f;
    float targetPillY = y0 + (float)g_tab * (h + 6.f);
    if (s_pillY < 0.f) s_pillY = targetPillY;
    s_pillY = AnimF(0x50494C4Cu, targetPillY, 16.f);

    for (int i = 0; i < 4; ++i) {
        bool sel = (g_tab == i);
        bool hov = ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + w, y + h));
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID(i);
        if (hov && !sel)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                              IM_COL32(255, 255, 255, 10), 8.f);
        float selT = AnimF(0x54414200u + (ImU32)i, sel ? 1.f : 0.f, 18.f);
        ImU32 col = LerpCol(kUDim, kUAccent, selT);
        if (g_font)
            dl->AddText(g_font, kSizeBase, ImVec2(x + 16.f, y + (h - kSizeBase) * 0.5f),
                        hov && selT < 0.5f ? LerpCol(col, kUText, 0.5f) : col, kTabs[i]);
        ImGui::InvisibleButton("##tab", ImVec2(w, h));
        if (ImGui::IsItemClicked(0) && g_tab != i) {
            g_tab = i;
            g_search[0] = 0;
        }
        ImGui::PopID();
        y += h + 6.f;
    }
    dl->AddRectFilled(ImVec2(x, s_pillY), ImVec2(x + w, s_pillY + h),
                      IM_COL32(96, 138, 250, 34), 8.f);
    dl->AddRectFilled(ImVec2(x + 1.f, s_pillY + 9.f), ImVec2(x + 4.f, s_pillY + h - 9.f),
                      kUAccent, 1.5f);

    if (g_smallFont)
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x + 16.f, p.y + s.y - 26.f), kUMuted,
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
    bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + h));

    // knob slides between the two ends, bg color fades in with it
    float t = AnimF(ImGui::GetID(id), v ? 1.f : 0.f, 20.f);
    ImU32 off = hov ? IM_COL32(56, 61, 72, 255) : IM_COL32(44, 48, 58, 255);
    ImU32 bg = LerpCol(off, kUAccent, t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
    float kx = p.x + 10.f + (w - 20.f) * t;
    float kr = 6.2f + 0.8f * t;
    dl->AddCircleFilled(ImVec2(kx, p.y + h * 0.5f), kr, IM_COL32(245, 247, 251, 255),
                        12);
    ImGui::InvisibleButton(id, ImVec2(w, h));
    if (ImGui::IsItemClicked(0)) v = !v;
    return v;
}

bool FancyButton(const char* label, const ImVec2& size, bool accent) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + size.x, p.y + size.y));
    float hovT = AnimF(ImGui::GetID("btn"), hov ? 1.f : 0.f, 16.f);
    ImU32 on = accent ? IM_COL32(118, 156, 252, 255) : IM_COL32(36, 40, 49, 255);
    ImU32 off = accent ? kUAccent : IM_COL32(27, 30, 38, 255);
    ImU32 bg = LerpCol(off, on, hovT);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 6.f);
    if (!accent)
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), kUStroke, 6.f, 0, 1.f);
    if (g_font) {
        ImVec2 ts = g_font->CalcTextSizeA(kSizeBase, FLT_MAX, 0.f, label);
        dl->AddText(g_font, kSizeBase,
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
    dl->AddText(g_font, kSizeBase, ImVec2(p.x, p.y + (desc ? 0.f : (h - kSizeBase) * 0.5f)),
                kUText, label);
    if (desc && g_smallFont)
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x, p.y + 21.f), kUDim, desc);
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - 38.f, p.y + (h - 20.f) * 0.5f));
    bool nv = DrawSwitch("##sw", *v);
    ImGui::PopID();
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
    dl->AddText(g_font, kSizeBase, ImVec2(p.x, p.y + (desc ? 0.f : (h - kSizeBase) * 0.5f)),
                kUText, label);
    if (desc && g_smallFont)
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x, p.y + 21.f), kUDim, desc);
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
    // accent bar grows out of the middle when the module turns on
    float barT = AnimF(ImGui::GetID("bar"), on ? 1.f : 0.f, 14.f);
    float barH = (h - 26.f) * barT;
    if (barH > 0.5f)
        dl->AddRectFilled(ImVec2(p.x + 2.f, p.y + (h - barH) * 0.5f),
                          ImVec2(p.x + 5.f, p.y + (h + barH) * 0.5f), kUAccent, 1.5f);
    if (g_font)
        dl->AddText(g_font, kSizeBase, ImVec2(p.x + 16.f, p.y + 11.f), kUText, m->Name());
    if (m->Desc() && m->Desc()[0] && g_smallFont)
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x + 16.f, p.y + 32.f), kUDim,
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
        ImVec2 s1 = g_font->CalcTextSizeA(kSizeBase, FLT_MAX, 0.f, t1);
        dl->AddText(g_font, kSizeBase, ImVec2(x + (w - s1.x) * 0.5f, y), kUDim, t1);
    }
    if (g_smallFont) {
        ImVec2 s2 = g_smallFont->CalcTextSizeA(kSizeSmall, FLT_MAX, 0.f, t2);
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(x + (w - s2.x) * 0.5f, y + 26.f),
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

    // Latin + Cyrillic + punctuation for RU UI text.
    static const ImWchar kGlyphRanges[] = {
        0x0020, 0x007E, // ASCII
        0x00A0, 0x024F, // Latin-1 + Latin Ext + IPA
        0x0370, 0x03FF, // Greek
        0x0400, 0x04FF, // Cyrillic
        0x2010, 0x2027, // punctuation
        0, 0
    };

    // Verdana TTF embedded in font_data.h: glyph coverage does not depend on
    // installed fonts. ImGui 1.92 dynamic fonts rasterize glyphs on demand;
    // we still pre-rasterize everything in WarmFonts() so the atlas does not
    // grow mid-frame (which drops already-recorded UVs -> missing letters).
    const unsigned ttfSize = (unsigned)sizeof(gui::kFontVerdana);
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.GlyphRanges = kGlyphRanges;
    g_font = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)ttfSize, kSizeBase, &cfg);
    if (!g_font) g_font = io.Fonts->AddFontDefault();
    g_smallFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)ttfSize, kSizeSmall, &cfg);
    if (!g_smallFont) g_smallFont = g_font;
    g_headFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)ttfSize, kSizeHead, &cfg);
    if (!g_headFont) g_headFont = g_font;
    g_titleFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)gui::kFontVerdana, (int)ttfSize, kSizeTitle, &cfg);
    if (!g_titleFont) g_titleFont = g_font;
    io.FontDefault = g_font;

    Log("[GUI] fonts: small=%p base=%p head=%p title=%p verdana=%u bytes",
        (void*)g_smallFont, (void*)g_font, (void*)g_headFont, (void*)g_titleFont,
        ttfSize);
    if (g_font && g_smallFont && g_headFont && g_titleFont) {
        Log("[GUI] font sizes: %.0f/%.0f/%.0f/%.0f (legacy)",
            g_smallFont->LegacySize, g_font->LegacySize, g_headFont->LegacySize,
            g_titleFont->LegacySize);
        Log("[GUI] glyph check base: latin=%d cyrA=%d cyrYa=%d",
            g_font->IsGlyphInFont('A') ? 1 : 0,
            g_font->IsGlyphInFont(0x0410) ? 1 : 0,
            g_font->IsGlyphInFont(0x044F) ? 1 : 0);
        Log("[GUI] glyph check small: latin=%d cyrA=%d",
            g_smallFont->IsGlyphInFont('A') ? 1 : 0,
            g_smallFont->IsGlyphInFont(0x0410) ? 1 : 0);
    } else {
        LogWarn("[GUI] font setup incomplete, some sizes fall back");
    }
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

// Force-rasterize every glyph we use into the ImGui 1.92 dynamic atlas.
// Must be called AFTER the first NewFrame() (Builder exists). Without this
// the atlas can grow mid-frame and already-recorded UVs go stale -> missing
// letters in the first frames.
void WarmFonts() {
    static const ImWchar kRanges[] = {
        0x0020, 0x007E,
        0x00A0, 0x024F,
        0x0370, 0x03FF,
        0x0400, 0x04FF,
        0x2010, 0x2027,
        0, 0
    };
    static bool s_logged = false;
    ImFont* fonts[] = { g_smallFont, g_font, g_headFont, g_titleFont };
    for (ImFont* f : fonts) {
        if (!f) continue;
        ImFontBaked* baked = f->GetFontBaked(f->LegacySize);
        if (!baked) {
            if (!s_logged)
                LogWarn("[GUI] WarmFonts: no baked for %s size=%.0f",
                        f->GetDebugName(), f->LegacySize);
            continue;
        }
        int total = 0, loaded = 0;
        for (const ImWchar* r = kRanges; r[0]; r += 2) {
            for (unsigned c = r[0]; c <= (unsigned)r[1]; ++c) {
                ++total;
                baked->FindGlyph((ImWchar)c);
                if (baked->IsGlyphLoaded((ImWchar)c)) ++loaded;
            }
        }
        if (!s_logged) {
            int tw = 0, th = 0;
            ImFontAtlas* atlas = f->ContainerAtlas;
            if (atlas && atlas->TexData) {
                tw = atlas->TexData->Width;
                th = atlas->TexData->Height;
            }
            Log("[GUI] WarmFonts %s size=%.0f glyphs=%d/%d atlas=%dx%d",
                f->GetDebugName(), f->LegacySize, loaded, total, tw, th);
            if (loaded < total / 4)
                LogWarn("[GUI] WarmFonts: only %d/%d glyphs loaded for %s",
                        loaded, total, f->GetDebugName());
        }
    }
    s_logged = true;
}

void OnMenuClosed() { g_menuAnim = 0.f; }

void DrawMainMenu() {
    UpdateKeyCapture();

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x < 320.f || io.DisplaySize.y < 240.f) return;

    Client& c = Client::Instance();

    // scale-in on open: the window grows from 90% while fading in
    g_menuAnim = AnimF(0x4D454E55u, 1.f, 14.f);
    float a = std::clamp(g_menuAnim, 0.f, 1.f);
    const ImVec2 size(800.f, 520.f);
    ImVec2 winSize(size.x * (0.90f + 0.10f * a), size.y * (0.90f + 0.10f * a));
    ImVec2 pos((io.DisplaySize.x - winSize.x) * 0.5f,
               (io.DisplaySize.y - winSize.y) * 0.5f);
    if (pos.x < 8.f) pos.x = 8.f;
    if (pos.y < 8.f) pos.y = 8.f;

    DrawMenuShadow(pos, winSize, a);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(winSize);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.75f + 0.25f * a);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##SummerMenu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (g_font) ImGui::PushFont(g_font, kSizeBase);

    DrawHeader();
    int tab = DrawTabs();

    float cx = pos.x + kSideW + 20.f;
    float cw = winSize.x - kSideW - 40.f;
    float cy = pos.y + kHeadH + 16.f;

    // content fades/slides in when the tab changes
    static int s_lastTab = 0;
    static float s_tabFade = 1.f;
    if (s_lastTab != tab) {
        s_lastTab = tab;
        s_tabFade = 0.f;
    }
    s_tabFade = AnimF(0x46414445u, 1.f, 12.f);
    float yoff = (1.f - s_tabFade) * 10.f;

    if (g_headFont)
        ImGui::GetWindowDrawList()->AddText(g_headFont, kSizeHead, ImVec2(cx, cy + 6.f),
                                            kUText, kTabs[tab]);

    if (tab < 3) {
        float sw = 210.f;
        ImGui::SetCursorScreenPos(ImVec2(cx + cw - sw, cy));
        ImGui::SetNextItemWidth(sw);
        ImGui::InputTextWithHint("##search", "search...", g_search, sizeof(g_search));
    }

    float by = cy + 40.f;
    float bh = pos.y + winSize.y - 16.f - by;
    ImGui::SetCursorScreenPos(ImVec2(cx, by + yoff));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f + 0.65f * s_tabFade);
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
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    if (g_font) ImGui::PopFont();
    ImGui::PopStyleVar(2);
}

void DrawWatermark() {
    if (!g_font || !g_smallFont) return;
    bool show = g_config.GetBool("client.watermark", true);
    static float s_wmA = 0.f;
    s_wmA = AnimF(0x574D414Eu, show ? 1.f : 0.f, 8.f);
    if (s_wmA < 0.015f) return;
    float a = std::clamp(s_wmA, 0.f, 1.f);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* name = "SUMMER CLIENT";
    char fps[24];
    snprintf(fps, sizeof fps, "%d fps", (int)(ImGui::GetIO().Framerate + 0.5f));
    ImVec2 ts = g_font->CalcTextSizeA(kSizeBase, FLT_MAX, 0.f, name);
    ImVec2 fs = g_smallFont->CalcTextSizeA(kSizeSmall, FLT_MAX, 0.f, fps);
    float h = 30.f;
    float w = 26.f + ts.x + 12.f + fs.x + 16.f;
    // slides in from off-screen left
    float x = 10.f - 40.f * (1.f - a);
    ImVec2 p0(x, 10.f), p1(x + w, 10.f + h);
    dl->AddRectFilled(p0, p1, IM_COL32(14, 16, 20, (int)(225 * a)), 8.f);
    dl->AddRect(p0, p1, IM_COL32(96, 138, 250, (int)(90 * a)), 8.f, 0, 1.f);
    dl->AddCircleFilled(ImVec2(p0.x + 15.f, p0.y + h * 0.5f), 3.5f,
                        IM_COL32(96, 138, 250, (int)(255 * a)));
    dl->AddText(g_font, kSizeBase, ImVec2(p0.x + 26.f, p0.y + (h - kSizeBase) * 0.5f),
                IM_COL32(232, 236, 243, (int)(255 * a)), name);
    dl->AddText(g_smallFont, kSizeSmall,
                ImVec2(p0.x + 26.f + ts.x + 12.f, p0.y + (h - kSizeSmall) * 0.5f),
                IM_COL32(96, 104, 120, (int)(255 * a)), fps);
}

ImDrawList* WorldDrawList() { return ImGui::GetBackgroundDrawList(); }

// ---------------------------------------------------------------------------

bool Checkbox(const char* label, bool* v) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 30.f;
    if (g_font)
        ImGui::GetWindowDrawList()->AddText(g_font, kSizeBase,
                                            ImVec2(p.x, p.y + (h - kSizeBase) * 0.5f),
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
    ImGui::TextColored(kAccent, "%.2f", *v);
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

    // pulse while waiting for a key
    float pulse = capture ? (0.6f + 0.4f * std::sin((float)ImGui::GetTime() * 6.0)) : 1.f;
    float hovT = AnimF(ImGui::GetID("kb"), capture ? 1.f : (hov ? 1.f : 0.f), 14.f);
    ImU32 bgOff = hov ? IM_COL32(38, 42, 52, 255) : IM_COL32(27, 30, 38, 255);
    ImU32 bg = LerpCol(bgOff, kUAccent, hovT * pulse);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, 6.f);
    ImU32 border = capture
                       ? LerpCol(kUStroke, kUAccent, pulse)
                       : (hov ? kUDim : kUStroke);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), border, 6.f, 0, 1.f);
    if (g_smallFont) {
        ImVec2 ts = g_smallFont->CalcTextSizeA(kSizeSmall, FLT_MAX, 0.f, buf);
        ImU32 tc = capture
                       ? IM_COL32(255, 255, 255, (int)(255 * pulse))
                       : (hov ? kUText : kUDim);
        dl->AddText(g_smallFont, kSizeSmall,
                    ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - kSizeSmall) * 0.5f), tc,
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
        dl->AddText(g_smallFont, kSizeSmall, ImVec2(p.x + 11.f, p.y + 1.f), kUDim, title);
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
    ImVec2 ts = g_smallFont->CalcTextSizeA(kSizeSmall, FLT_MAX, w, text);
    ImGui::GetWindowDrawList()->AddText(g_smallFont, kSizeSmall, p, kUDim, text, NULL, w);
    ImGui::Dummy(ImVec2(0, ts.y + 3.f));
}

}  // namespace gui
}  // namespace summer
