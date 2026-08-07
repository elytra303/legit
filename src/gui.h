#pragma once
#include <windows.h>

namespace gui {
bool init(HWND hwnd, int screen_w, int screen_h);
void render();
void shutdown();
}
