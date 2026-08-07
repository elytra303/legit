#include "autoswap.h"

#include <string>

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

AutoSwap::AutoSwap()
    : Module("AutoSwap", "Swaps to the best hotbar weapon while fighting",
             Category::Combat) {}

void AutoSwap::Save(Config& c) {
    c.SetBool("autoswap.enabled", Enabled());
    c.SetInt("autoswap.key", Key());
    c.SetBool("autoswap.onlyInCombat", onlyInCombat_);
    c.SetBool("autoswap.switchBack", switchBack_);
    c.SetBool("autoswap.players", targetPlayers_);
    c.SetBool("autoswap.mobs", targetMobs_);
    c.SetBool("autoswap.ignoreTeam", ignoreTeammates_);
    c.SetDouble("autoswap.range", range_);
}

void AutoSwap::Load(const Config& c) {
    SetEnabled(c.GetBool("autoswap.enabled", false));
    SetKey(c.GetInt("autoswap.key", 0));
    onlyInCombat_ = c.GetBool("autoswap.onlyInCombat", true);
    switchBack_ = c.GetBool("autoswap.switchBack", true);
    targetPlayers_ = c.GetBool("autoswap.players", true);
    targetMobs_ = c.GetBool("autoswap.mobs", false);
    ignoreTeammates_ = c.GetBool("autoswap.ignoreTeam", true);
    range_ = (float)c.GetDouble("autoswap.range", 5.0);
}

bool AutoSwap::HasTargetNear(mc::WorldSnapshot& snap) {
    for (auto& e : snap.entities) {
        if (e.isLocal || !e.isAlive) continue;
        if (ignoreTeammates_ && e.isPlayer && e.allied) continue;
        if (e.isPlayer) {
            if (!targetPlayers_) continue;
        } else {
            if (!targetMobs_) continue;
            if (!e.hostile) continue;
        }
        if (e.dist <= range_) return true;
    }
    return false;
}

int AutoSwap::ScoreItem(const std::string& n) {
    int s = 0;
    if (n.find("sword") != std::string::npos)
        s += 10;
    else if (n.find("mace") != std::string::npos)
        s += 9;
    else if (n.find("axe") != std::string::npos)
        s += 8;
    else if (n.find("trident") != std::string::npos)
        s += 7;
    else
        s -= 6;
    if (n.find("netherite") != std::string::npos)
        s += 5;
    else if (n.find("diamond") != std::string::npos)
        s += 4;
    else if (n.find("iron") != std::string::npos)
        s += 2;
    else if (n.find("golden") != std::string::npos || n.find("stone") != std::string::npos)
        s += 1;
    return s;
}

void AutoSwap::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) {
        inCombat_ = false;
        switching_ = false;
        backing_ = false;
        return;
    }

    bool near = HasTargetNear(snap);
    if (!near) {
        if (inCombat_ && switchBack_ && prevSlot_ >= 0 && bestSlot_ >= 0) {
            if (!backing_) {
                backing_ = true;
                backDelay_ = Rng::Range(120, 260);
                backTimer_.Reset();
            } else if (backTimer_.Elapsed(backDelay_)) {
                mc::SetSelectedSlot(prevSlot_);
                backing_ = false;
                prevSlot_ = -1;
                bestSlot_ = -1;
            }
        }
        inCombat_ = false;
        switching_ = false;
        return;
    }

    inCombat_ = true;
    if (!onlyInCombat_ && !near) return;

    int cur = mc::GetSelectedSlot();
    if (cur < 0) return;

    int best = -1, bestScore = -1;
    for (int i = 0; i < 9; ++i) {
        std::string name;
        if (!mc::GetHotbarItemName(i, name)) continue;
        int sc = ScoreItem(name);
        if (i == cur) sc += 2;  // prefer the slot we already hold
        if (sc > bestScore) {
            bestScore = sc;
            best = i;
        }
    }
    if (best < 0 || best == cur) {
        switching_ = false;
        prevSlot_ = -1;
        bestSlot_ = -1;
        return;
    }

    if (!switching_) {
        switching_ = true;
        prevSlot_ = cur;
        bestSlot_ = best;
        switchDelay_ = Rng::Range(30, 90);
        switchTimer_.Reset();
    } else if (switchTimer_.Elapsed(switchDelay_)) {
        mc::SetSelectedSlot(bestSlot_);
        switching_ = false;
    }
}

void AutoSwap::DrawSettings() {
    gui::Section("TARGETS");
    gui::Checkbox("Players", &targetPlayers_);
    gui::Checkbox("Mobs", &targetMobs_);
    gui::Checkbox("Ignore teammates", &ignoreTeammates_);
    gui::SliderFloat("Range", &range_, 1.f, 8.f);
    gui::Separator();
    gui::Section("BEHAVIOR");
    gui::Checkbox("Only in combat", &onlyInCombat_);
    gui::Checkbox("Switch back after", &switchBack_);
    gui::Help("Scores sword > mace > axe > trident, then material");
}

}  // namespace summer
