#include "client.h"

#include <windows.h>

#include "mc/minecraft.h"
#include "util/log.h"
#include "util/timer.h"

namespace summer {

Client& Client::Instance() {
    static Client c;
    return c;
}

void Client::AddModule(Module* m) { modules_.push_back(m); }

void Client::Initialize() {
    Log("[Summer] initializing client");
    LoadConfig();
    for (auto* m : modules_) {
        m->Load(g_config);
        if (m->Enabled()) m->OnEnable();
    }
}

void Client::Shutdown() {
    SaveConfig();
    for (auto* m : modules_)
        if (m->Enabled()) m->OnDisable();
}

void Client::SaveConfig() {
    g_config.SetBool("client.menu", menuOpen_);
    for (auto* m : modules_) m->Save(g_config);
    g_config.Save();
}

void Client::LoadConfig() {
    g_config.Load();
    menuOpen_ = g_config.GetBool("client.menu", false);
    for (auto* m : modules_) m->Load(g_config);
}

void Client::OnFrame() {
    snapshot_ = mc::Capture();
    inGame_ = snapshot_.valid;

    // module hotkeys
    static unsigned char prevKeys[256] = {0};
    for (auto* m : modules_) {
        int vk = m->Key();
        if (vk <= 0 || vk >= 256) continue;
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !prevKeys[vk]) m->Toggle();
        prevKeys[vk] = down ? 1 : 0;
    }

    if (menuOpen_) {
        mc::ClearScreenInput();  // block game input while our GUI is open
        return;                  // don't run combat/movement while menu is up
    }
    if (!inGame_) return;

    for (auto* m : modules_) {
        if (!m->Enabled()) continue;
        m->OnFrame();
    }
}

}  // namespace summer
