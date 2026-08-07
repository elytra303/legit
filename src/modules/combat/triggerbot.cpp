#include "triggerbot.h"

#include <windows.h>

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

TriggerBot::TriggerBot()
    : Module("TriggerBot",
             "Attacks automatically when the crosshair is on an enemy hitbox",
             Category::Combat) {}

void TriggerBot::Save(Config& c) {
    c.SetBool("trigger.enabled", Enabled());
    c.SetInt("trigger.key", Key());
    c.SetBool("trigger.players", targetPlayers_);
    c.SetBool("trigger.mobs", targetMobs_);
    c.SetBool("trigger.ignoreTeam", ignoreTeammates_);
    c.SetBool("trigger.ignoreInvis", ignoreInvisible_);
    c.SetBool("trigger.requireKey", requireKey_);
    c.SetDouble("trigger.range", range_);
    c.SetDouble("trigger.fov", fov_);
    c.SetInt("trigger.delayMin", delayMinMs_);
    c.SetInt("trigger.delayMax", delayMaxMs_);
}

void TriggerBot::Load(const Config& c) {
    SetEnabled(c.GetBool("trigger.enabled", false));
    SetKey(c.GetInt("trigger.key", 0));
    targetPlayers_ = c.GetBool("trigger.players", true);
    targetMobs_ = c.GetBool("trigger.mobs", false);
    ignoreTeammates_ = c.GetBool("trigger.ignoreTeam", true);
    ignoreInvisible_ = c.GetBool("trigger.ignoreInvis", true);
    requireKey_ = c.GetBool("trigger.requireKey", false);
    range_ = (float)c.GetDouble("trigger.range", 4.0);
    fov_ = (float)c.GetDouble("trigger.fov", 4.0);
    delayMinMs_ = c.GetInt("trigger.delayMin", 40);
    delayMaxMs_ = c.GetInt("trigger.delayMax", 140);
}

mc::EntitySnap* TriggerBot::RaycastTarget(mc::WorldSnapshot& snap) {
    Ray ray{snap.eyePos, DirectionFromRot(snap.localYaw, snap.localPitch)};
    mc::EntitySnap* best = nullptr;
    double bestT = 1e18;
    for (auto& e : snap.entities) {
        if (e.isLocal) continue;
        if (!e.isAlive) continue;
        if (ignoreInvisible_ && e.isInvisible) continue;
        if (e.isPlayer) {
            if (!targetPlayers_) continue;
            if (ignoreTeammates_ && e.allied) continue;
        } else {
            if (!targetMobs_) continue;
            if (!e.hostile) continue;
        }
        if (e.dist > range_) continue;
        if (e.fovAngle > fov_) continue;

        AABB box = e.box;
        box.minX -= 0.1; box.maxX += 0.1;
        box.minY -= 0.1; box.maxY += 0.1;
        box.minZ -= 0.1; box.maxZ += 0.1;

        double tn, tf;
        if (RayAABB(ray, box, tn, tf)) {
            if (tn < bestT) {
                bestT = tn;
                best = &e;
            }
        }
    }
    return best;
}

void TriggerBot::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) {
        waiting_ = false;
        return;
    }

    bool keyHeld =
        requireKey_ ? (Key() != 0 && (GetAsyncKeyState(Key()) & 0x8000)) : true;
    if (!keyHeld) {
        waiting_ = false;
        return;
    }

    // weapon cooldown between shots
    if (!afterTimer_.Elapsed(afterDelay_)) {
        waiting_ = false;
        return;
    }

    if (!waiting_) {
        if (scanTimer_.Elapsed(Rng::Range(15, 45))) {
            scanTimer_.Reset();
            mc::EntitySnap* t = RaycastTarget(snap);
            if (t && t->attackCooldownPct >= 80) {
                waiting_ = true;
                fireDelay_ = Rng::Range(delayMinMs_, delayMaxMs_);
                fireTimer_.Reset();
            }
        }
        return;
    }

    if (fireTimer_.Elapsed(fireDelay_)) {
        mc::EntitySnap* t = RaycastTarget(snap);
        if (t) {
            mc::Attack(t->ref);
            afterDelay_ = Rng::Range(280, 500);
            afterTimer_.Reset();
        }
        waiting_ = false;
    }
}

void TriggerBot::DrawSettings() {
    gui::Section("TARGETS");
    gui::Checkbox("Players", &targetPlayers_);
    gui::Checkbox("Mobs", &targetMobs_);
    gui::Checkbox("Ignore teammates", &ignoreTeammates_);
    gui::Checkbox("Ignore invisible", &ignoreInvisible_);
    gui::SliderFloat("Range", &range_, 1.f, 6.f);
    gui::SliderFloat("FOV", &fov_, 0.5f, 20.f);
    gui::Separator();
    gui::Section("TIMING");
    gui::Checkbox("Hold key to fire", &requireKey_);
    gui::Help("Module key = trigger key when 'Hold key' is on");
    gui::SliderInt("Delay min ms", &delayMinMs_, 0, 500);
    gui::SliderInt("Delay max ms", &delayMaxMs_, 0, 500);
}

}  // namespace summer
