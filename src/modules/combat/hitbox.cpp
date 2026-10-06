#include "hitbox.h"

#include <algorithm>

#include "../../client.h"
#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"

namespace summer {

HitBoxModule::HitBoxModule()
    : Module("HitBox",
             "Enlarges entity hitboxes client side for easier hits",
             Category::Combat) {}

void HitBoxModule::Save(Config& c) {
    c.SetBool("hitbox.enabled", Enabled());
    c.SetInt("hitbox.key", Key());
    c.SetDouble("hitbox.expand", expand_);
    c.SetDouble("hitbox.expandY", expandY_);
    c.SetBool("hitbox.players", players_);
    c.SetBool("hitbox.mobs", mobs_);
    c.SetBool("hitbox.ignoreTeam", ignoreTeammates_);
    c.SetBool("hitbox.ignoreInvis", ignoreInvisible_);
    c.SetBool("hitbox.reachOnly", reachOnly_);
    c.SetBool("hitbox.crosshair", crosshairOnly_);
    c.SetBool("hitbox.jitter", jitter_);
    c.SetDouble("hitbox.range", range_);
    c.SetDouble("hitbox.fov", fov_);
}

void HitBoxModule::Load(const Config& c) {
    SetEnabled(c.GetBool("hitbox.enabled", false));
    SetKey(c.GetInt("hitbox.key", 0));
    expand_ = (float)c.GetDouble("hitbox.expand", 0.25);
    expandY_ = (float)c.GetDouble("hitbox.expandY", 0.1);
    players_ = c.GetBool("hitbox.players", true);
    mobs_ = c.GetBool("hitbox.mobs", true);
    ignoreTeammates_ = c.GetBool("hitbox.ignoreTeam", true);
    ignoreInvisible_ = c.GetBool("hitbox.ignoreInvis", true);
    reachOnly_ = c.GetBool("hitbox.reachOnly", true);
    crosshairOnly_ = c.GetBool("hitbox.crosshair", true);
    jitter_ = c.GetBool("hitbox.jitter", true);
    range_ = (float)c.GetDouble("hitbox.range", 4.0);
    fov_ = (float)c.GetDouble("hitbox.fov", 10.0);
}

void HitBoxModule::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) return;

    for (auto& e : snap.entities) {
        if (e.isLocal || !e.isAlive || !e.ref) continue;
        if (e.isPlayer) {
            if (!players_) continue;
            if (ignoreTeammates_ && e.allied) continue;
        } else {
            if (!mobs_) continue;
            if (ignoreInvisible_ && e.isInvisible) continue;
        }
        if (e.dist > range_) continue;
        // keep the expanded box inside vanilla reach so the distance check
        // on the server still passes
        if (reachOnly_ && e.dist > 3.0) continue;
        if (crosshairOnly_ && e.fovAngle > fov_) continue;

        float gx = expand_, gy = expandY_;
        if (jitter_) {
            gx *= (float)Rng::Range(0.85, 1.15);
            gy *= (float)Rng::Range(0.85, 1.15);
        }
        mc::ExpandEntityBox(e.ref, std::max(gx, 0.f), std::max(gy, 0.f));
    }
}

void HitBoxModule::DrawSettings() {
    gui::Section("SIZE");
    gui::SliderFloat("Horizontal", &expand_, 0.f, 0.6f);
    gui::SliderFloat("Vertical", &expandY_, 0.f, 0.3f);
    gui::Help("added around each entity on your client only");
    gui::Separator();
    gui::Section("TARGETS");
    gui::Checkbox("Players", &players_);
    gui::Checkbox("Mobs", &mobs_);
    gui::Checkbox("Ignore teammates", &ignoreTeammates_);
    gui::Checkbox("Ignore invisible", &ignoreInvisible_);
    gui::SliderFloat("Range", &range_, 1.f, 6.f);
    gui::Separator();
    gui::Section("BYPASS");
    gui::Checkbox("Reach only", &reachOnly_);
    gui::Help("skips entities farther than vanilla reach (3.0)");
    gui::Checkbox("Crosshair only", &crosshairOnly_);
    gui::SliderFloat("FOV", &fov_, 1.f, 45.f);
    gui::Checkbox("Jitter", &jitter_);
    gui::Help("slight random variation so the box is never constant");
}

}  // namespace summer
