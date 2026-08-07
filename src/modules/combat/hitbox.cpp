#include "hitbox.h"

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

HitboxModule::HitboxModule()
    : Module("Hitbox", "Aims at the real hitbox - smooth (normal) or snap-and-return (legit)",
             Category::Combat) {}

void HitboxModule::Save(Config& c) {
    c.SetBool("hitbox.enabled", Enabled());
    c.SetInt("hitbox.key", Key());
    c.SetInt("hitbox.mode", mode_);
    c.SetBool("hitbox.onlyWhileAttack", onlyWhileAttack_);
    c.SetBool("hitbox.players", targetPlayers_);
    c.SetBool("hitbox.mobs", targetMobs_);
    c.SetBool("hitbox.ignoreTeam", ignoreTeammates_);
    c.SetBool("hitbox.ignoreInvis", ignoreInvisible_);
    c.SetDouble("hitbox.range", range_);
    c.SetDouble("hitbox.fov", fov_);
    c.SetInt("hitbox.hitpoint", hitPoint_);
    c.SetBool("hitbox.randomize", randomizePoint_);
    c.SetDouble("hitbox.smooth", smooth_);
    c.SetDouble("hitbox.snapSpeed", snapSpeed_);
    c.SetDouble("hitbox.returnSpeed", returnSpeed_);
    c.SetInt("hitbox.postDelay", postHitDelayMs_);
    c.SetBool("hitbox.autoAttack", autoAttack_);
    c.SetBool("hitbox.returnToOrigin", returnToOrigin_);
    c.SetDouble("hitbox.headChance", headChance_);
    c.SetDouble("hitbox.chestChance", chestChance_);
}

void HitboxModule::Load(const Config& c) {
    SetEnabled(c.GetBool("hitbox.enabled", false));
    SetKey(c.GetInt("hitbox.key", 0));
    mode_ = c.GetInt("hitbox.mode", 1);
    onlyWhileAttack_ = c.GetBool("hitbox.onlyWhileAttack", true);
    targetPlayers_ = c.GetBool("hitbox.players", true);
    targetMobs_ = c.GetBool("hitbox.mobs", true);
    ignoreTeammates_ = c.GetBool("hitbox.ignoreTeam", true);
    ignoreInvisible_ = c.GetBool("hitbox.ignoreInvis", true);
    range_ = (float)c.GetDouble("hitbox.range", 4.5);
    fov_ = (float)c.GetDouble("hitbox.fov", 25.0);
    hitPoint_ = c.GetInt("hitbox.hitpoint", 0);
    randomizePoint_ = c.GetBool("hitbox.randomize", true);
    smooth_ = (float)c.GetDouble("hitbox.smooth", 30.0);
    snapSpeed_ = (float)c.GetDouble("hitbox.snapSpeed", 60.0);
    returnSpeed_ = (float)c.GetDouble("hitbox.returnSpeed", 20.0);
    postHitDelayMs_ = c.GetInt("hitbox.postDelay", 120);
    autoAttack_ = c.GetBool("hitbox.autoAttack", true);
    returnToOrigin_ = c.GetBool("hitbox.returnToOrigin", true);
    headChance_ = (float)c.GetDouble("hitbox.headChance", 40.0);
    chestChance_ = (float)c.GetDouble("hitbox.chestChance", 45.0);
}

static float AngleDiff(float a, float b) {
    float d = std::fmod(a - b, 360.f);
    if (d > 180.f) d -= 360.f;
    if (d < -180.f) d += 360.f;
    return d;
}

mc::EntitySnap* HitboxModule::PickTarget(mc::WorldSnapshot& snap) {
    mc::EntitySnap* best = nullptr;
    double bestScore = 1e18;
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
        if (e.dist < bestScore) {
            bestScore = e.dist;
            best = &e;
        }
    }
    return best;
}

Vec3 HitboxModule::PickHitPoint(const mc::EntitySnap& e) {
    const AABB& b = e.box;
    Vec3 c = b.Center();
    double jx = randomizePoint_ ? Rng::Range(-0.12, 0.12) : 0.0;
    double jz = randomizePoint_ ? Rng::Range(-0.12, 0.12) : 0.0;
    double jy = randomizePoint_ ? Rng::Range(-0.04, 0.04) : 0.0;
    double h = b.maxY - b.minY;
    switch (hitPoint_) {
        case 1:
            return {c.x + jx, b.maxY - 0.12 + jy, c.z + jz};
        case 2:
            return {c.x + jx, b.minY + h * 0.65 + jy, c.z + jz};
        case 3:
            return {c.x + jx, b.minY + 0.08, c.z + jz};
        case 4:
            return {c.x, b.minY + h * 0.5, c.z};
        default: {
            double r = Rng::Next() * 100.0;
            double frac;
            if (r < headChance_)
                frac = 0.88;
            else if (r < headChance_ + chestChance_)
                frac = 0.60;
            else
                frac = 0.15;
            return {c.x + Rng::Range(-0.16, 0.16), b.minY + h * frac + Rng::Range(-0.05, 0.05),
                    c.z + Rng::Range(-0.16, 0.16)};
        }
    }
}

void HitboxModule::CalcAngles(const Vec3& from, const Vec3& to, float& yaw,
                              float& pitch) {
    Vec3 d = to - from;
    yaw = (float)(std::atan2(-d.x, d.z) * 180.0 / kPi);
    double horiz = std::sqrt(d.x * d.x + d.z * d.z);
    pitch = (float)(std::atan2(-d.y, horiz) * 180.0 / kPi);
}

void HitboxModule::BeginReturn() {
    if (!returnToOrigin_) {
        state_ = State::Idle;
        return;
    }
    returnYaw_ = originYaw_;
    returnPitch_ = originPitch_;
    if (Rng::Chance(25)) {
        returnYaw_ += (float)Rng::Range(-2.5, 2.5);
        returnPitch_ += (float)Rng::Range(-1.5, 1.5);
    }
    returnDeg_ = 0.35f + returnSpeed_ * 0.05f;
    postHitTimer_.Reset();
    state_ = State::Return;
}

void HitboxModule::StepReturn(mc::WorldSnapshot& snap) {
    if (!postHitTimer_.Elapsed(postHitDelayMs_)) return;  // brief hold after the hit
    float cy = snap.localYaw, cp = snap.localPitch;
    float dy = AngleDiff(returnYaw_, cy);
    float dp = AngleDiff(returnPitch_, cp);
    if (std::fabs(dy) < 0.4f && std::fabs(dp) < 0.4f) {
        mc::SetRotation(originYaw_, originPitch_);
        state_ = State::Idle;
        return;
    }
    mc::SetRotation(cy + std::clamp(dy, -returnDeg_, returnDeg_),
                    cp + std::clamp(dp, -returnDeg_, returnDeg_));
}

void HitboxModule::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) {
        state_ = State::Idle;
        return;
    }

    bool attackHeld = mc::IsKeyAttackDown();
    bool want = (!onlyWhileAttack_ || attackHeld);

    if (mode_ == 0) {
        // ---------------- NORMAL: smooth stick aim ----------------
        if (!want) return;
        mc::EntitySnap* t = PickTarget(snap);
        if (!t) return;
        if (t->fovAngle > fov_) return;
        Vec3 aimAt = PickHitPoint(*t);
        float ty, tp;
        CalcAngles(snap.eyePos, aimAt, ty, tp);
        float cy = snap.localYaw, cp = snap.localPitch;
        float f = std::clamp(smooth_ / 100.f, 0.01f, 1.f);
        mc::SetRotation(cy + AngleDiff(ty, cy) * f, cp + AngleDiff(tp, cp) * f);
        return;
    }

    // ---------------- LEGIT: snap -> hit -> return ----------------
    switch (state_) {
        case State::Idle: {
            mc::EntitySnap* t = PickTarget(snap);
            if (!t || !want) return;
            Vec3 aimAt = PickHitPoint(*t);
            float ty, tp;
            CalcAngles(snap.eyePos, aimAt, ty, tp);
            float cy = snap.localYaw, cp = snap.localPitch;
            if (std::fabs(AngleDiff(ty, cy)) < 1.0f && std::fabs(AngleDiff(tp, cp)) < 1.0f)
                return;  // already on target, nothing to do
            lastAimAt_ = aimAt;
            lastTargetCenter_ = t->pos;
            originYaw_ = cy;
            originPitch_ = cp;
            targetYaw_ = ty;
            targetPitch_ = tp;
            snapDeg_ = 1.2f + snapSpeed_ * 0.12f;
            attacked_ = false;
            state_ = State::Snap;
            break;
        }
        case State::Snap: {
            mc::EntitySnap* t = PickTarget(snap);
            if (!t || !want) {
                BeginReturn();
                break;
            }
            // refresh aim point if the target moved
            if ((t->pos - lastTargetCenter_).Length() > 0.6) {
                lastTargetCenter_ = t->pos;
                lastAimAt_ = PickHitPoint(*t);
                CalcAngles(snap.eyePos, lastAimAt_, targetYaw_, targetPitch_);
            }
            float cy = snap.localYaw, cp = snap.localPitch;
            float dy = AngleDiff(targetYaw_, cy);
            float dp = AngleDiff(targetPitch_, cp);
            if (std::fabs(dy) < 0.35f && std::fabs(dp) < 0.35f) {
                if (!attacked_ && autoAttack_) {
                    if (t->attackCooldownPct >= 80) {
                        mc::Attack(t->ref);
                        attacked_ = true;
                    }
                }
                postHitTimer_.Reset();
                BeginReturn();
                break;
            }
            mc::SetRotation(cy + std::clamp(dy, -snapDeg_, snapDeg_),
                            cp + std::clamp(dp, -snapDeg_, snapDeg_));
            break;
        }
        case State::Return:
            StepReturn(snap);
            break;
    }
}

void HitboxModule::DrawSettings() {
    static const char* modes[] = {"Normal", "Legit"};
    static const char* points[] = {"Random", "Head", "Chest", "Feet", "Center"};
    gui::Combo("Mode", &mode_, modes, 2);
    gui::Checkbox("Only while attacking", &onlyWhileAttack_);
    gui::Separator();
    gui::Section("TARGETS");
    gui::Checkbox("Players", &targetPlayers_);
    gui::Checkbox("Mobs", &targetMobs_);
    gui::Checkbox("Ignore teammates", &ignoreTeammates_);
    gui::Checkbox("Ignore invisible", &ignoreInvisible_);
    gui::SliderFloat("Range", &range_, 2.f, 8.f);
    gui::Separator();
    gui::Section("HITPOINT");
    gui::Combo("Hit point", &hitPoint_, points, 5);
    gui::Checkbox("Randomize point", &randomizePoint_);
    if (hitPoint_ == 0) {
        gui::SliderFloat("Head chance %", &headChance_, 0.f, 100.f);
        gui::SliderFloat("Chest chance %", &chestChance_, 0.f, 100.f);
    }
    gui::Separator();
    gui::Section("AIM");
    if (mode_ == 0) {
        gui::SliderFloat("FOV", &fov_, 1.f, 180.f);
        gui::SliderFloat("Smoothness", &smooth_, 1.f, 100.f);
    } else {
        gui::SliderFloat("Snap speed", &snapSpeed_, 10.f, 100.f);
        gui::SliderFloat("Return speed", &returnSpeed_, 5.f, 80.f);
        gui::SliderInt("Post-hit delay ms", &postHitDelayMs_, 0, 500);
        gui::Checkbox("Auto attack", &autoAttack_);
        gui::Checkbox("Return to camera", &returnToOrigin_);
    }
}

}  // namespace summer
