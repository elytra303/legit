#pragma once

#include <imgui.h>

namespace summer {
namespace gui {

void ApplyTheme();
void DrawMainMenu();
void DrawWatermark();
ImDrawList* WorldDrawList();

bool Checkbox(const char* label, bool* v);
bool SliderInt(const char* label, int* v, int mn, int mx);
bool SliderFloat(const char* label, float* v, float mn, float mx);
bool Combo(const char* label, int* idx, const char* const* items, int count);
bool Keybind(const char* label, int* key);
bool KeybindButton(int* key);
void Section(const char* title);
void Help(const char* text);
const char* KeyName(int vk);

}  // namespace gui
}  // namespace summer
