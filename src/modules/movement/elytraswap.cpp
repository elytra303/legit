#include "elytraswap.h"

#include <string>
#include <windows.h>

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

ElytraSwapModule::ElytraSwapModule()
    : Module("ElytraSwap",
             "Swaps your chestplate for an elytra (and back) with a single click",
             Category::Movement) {}

void ElytraSwapModule::Save(Config& c) {
    c.SetBool("elytraswap.enabled", Enabled());
    c.SetInt("elytraswap.key", Key());
    c.SetInt("elytraswap.mode", mode_);
    c.SetInt("elytraswap.swapKey", swapKey_);
    c.SetBool("elytraswap.humanize", humanize_);
    c.SetInt("elytraswap.reactMin", reactMin_);
    c.SetInt("elytraswap.reactMax", reactMax_);
}

void ElytraSwapModule::Load(const Config& c) {
    SetEnabled(c.GetBool("elytraswap.enabled", false));
    SetKey(c.GetInt("elytraswap.key", 0));
    mode_ = c.GetInt("elytraswap.mode", 1);
    swapKey_ = c.GetInt("elytraswap.swapKey", 0);
    humanize_ = c.GetBool("elytraswap.humanize", true);
    reactMin_ = c.GetInt("elytraswap.reactMin", 60);
    reactMax_ = c.GetInt("elytraswap.reactMax", 140);
    if (reactMax_ < reactMin_) reactMax_ = reactMin_;
}

double ElytraSwapModule::Jitter(double a, double b) {
    return humanize_ ? Rng::Range(a, b) : (a + b) * 0.5;
}

bool ElytraSwapModule::IsChestElytra() {
    std::string n;
    return mc::GetArmorItemName(2, n) && n == "item.minecraft.elytra";
}

bool ElytraSwapModule::IsChestplate(const std::string& n) {
    return n.find("_chestplate") != std::string::npos;
}

int ElytraSwapModule::FindItem(const std::string& exactId) {
    for (int i = 0; i < 36; ++i) {
        std::string n;
        if (!mc::GetInventoryItemName(i, n)) continue;
        if (n == exactId) return i;
    }
    return -1;
}

int ElytraSwapModule::FindChestplate() {
    for (int i = 0; i < 36; ++i) {
        std::string n;
        if (!mc::GetInventoryItemName(i, n)) continue;
        if (IsChestplate(n)) return i;
    }
    return -1;
}

void ElytraSwapModule::Begin(Action a) {
    pending_ = a;
    stage_ = Stage::Delay;
    delayMs_ = Jitter((double)reactMin_, (double)reactMax_);
    actionTimer_.Reset();
}

void ElytraSwapModule::RunStage() {
    if (stage_ == Stage::Idle) return;

    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid || snap.hasScreen) {
        stage_ = Stage::Idle;
        pending_ = Action::None;
        return;
    }

    if (stage_ == Stage::Delay) {
        if (!actionTimer_.Elapsed(delayMs_)) return;
        bool ok = false;
        if (pending_ == Action::Equip) {
            if (!IsChestElytra()) {
                int slot = FindItem("item.minecraft.elytra");
                if (slot >= 0) ok = mc::InventoryQuickMove(0, slot);
            }
        } else if (pending_ == Action::Unequip) {
            if (IsChestElytra()) {
                int slot = FindChestplate();
                if (slot >= 0)
                    ok = mc::InventoryQuickMove(0, slot);
                else
                    ok = mc::InventoryQuickMove(0, 38);  // take the elytra off
            }
        }
        if (!ok) {
            stage_ = Stage::Idle;
            pending_ = Action::None;
            return;
        }
        stage_ = Stage::Cooldown;
        cooldownMs_ = Jitter(350, 600);
        actionTimer_.Reset();
        return;
    }

    if (stage_ == Stage::Cooldown) {
        if (actionTimer_.Elapsed(cooldownMs_)) {
            stage_ = Stage::Idle;
            pending_ = Action::None;
        }
    }
}

void ElytraSwapModule::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid || snap.hasScreen) {
        stage_ = Stage::Idle;
        pending_ = Action::None;
        wasJump_ = false;
        swapKeyPrev_ = false;
        falling_ = false;
        return;
    }

    bool jump = mc::IsKeyKindDown(KeyKind::Jump);

    // mode 0: dedicated swap key (rising edge)
    if (mode_ == 0 && swapKey_ > 0) {
        bool down = (GetAsyncKeyState(swapKey_) & 0x8000) != 0;
        if (down && !swapKeyPrev_ && stage_ == Stage::Idle) {
            if (IsChestElytra())
                Begin(Action::Unequip);
            else
                Begin(Action::Equip);
        }
        swapKeyPrev_ = down;
    }

    // modes 1 & 2: double-tap jump toggles elytra (vanilla-style deploy)
    if (mode_ >= 1) {
        if (jump && !wasJump_) {
            if (jumps_ >= 1 && jumpTimer_.ElapsedMs() < 400.0) {
                if (stage_ == Stage::Idle) {
                    if (IsChestElytra())
                        Begin(Action::Unequip);
                    else
                        Begin(Action::Equip);
                }
                jumps_ = 0;
            } else {
                jumps_ = 1;
                jumpTimer_.Reset();
            }
        }
        wasJump_ = jump;
    }

    // mode 2: swap back to the chestplate when we land
    if (mode_ == 2) {
        double y = snap.localPos.y;
        double dy = y - prevY_;
        if (dy < -0.02)
            falling_ = true;
        else if (dy > -0.01 && falling_ && !jump && stage_ == Stage::Idle &&
                 IsChestElytra()) {
            Begin(Action::Unequip);
            falling_ = false;
        }
        prevY_ = y;
    }

    RunStage();
}

void ElytraSwapModule::DrawSettings() {
    static const char* modes[] = {"Swap key", "Double jump", "Auto"};
    gui::Combo("Mode", &mode_, modes, 3);
    if (mode_ == 0) gui::Keybind("Swap key", &swapKey_);
    gui::Help("Double jump mimics the vanilla elytra deploy");
    gui::Separator();
    gui::Section("TIMING");
    gui::SliderInt("React min ms", &reactMin_, 20, 400);
    gui::SliderInt("React max ms", &reactMax_, 20, 400);
    gui::Checkbox("Humanize", &humanize_);
    gui::Help("Swaps using a real inventory click (shift-click)");
}

}  // namespace summer
