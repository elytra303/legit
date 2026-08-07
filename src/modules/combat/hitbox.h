#pragma once

#include "../../math/math.h"
#include "../../mc/types.h"
#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class HitboxModule : public Module {
public:
    HitboxModule();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    enum class State { Idle, Snap, Return };

    int mode_ = 1;  // 0 = normal aim, 1 = legit snap-and-return
    bool onlyWhileAttack_ = true;
    bool targetPlayers_ = true;
    bool targetMobs_ = true;
    bool ignoreTeammates_ = true;
    bool ignoreInvisible_ = true;
    float range_ = 4.5f;
    float fov_ = 25.f;   // normal mode fov limit
    int hitPoint_ = 0;   // 0 random, 1 head, 2 chest, 3 feet, 4 center
    bool randomizePoint_ = true;
    float smooth_ = 30.f;        // normal mode smoothing (%)
    float snapSpeed_ = 60.f;     // legit snap speed (%)
    float returnSpeed_ = 20.f;   // legit return speed (%)
    int postHitDelayMs_ = 120;
    bool autoAttack_ = true;
    bool returnToOrigin_ = true;
    float headChance_ = 40.f, chestChance_ = 45.f;

    State state_ = State::Idle;
    Vec3 lastAimAt_;
    Vec3 lastTargetCenter_;
    float originYaw_ = 0, originPitch_ = 0;
    float targetYaw_ = 0, targetPitch_ = 0;
    float returnYaw_ = 0, returnPitch_ = 0;
    float snapDeg_ = 6.f, returnDeg_ = 1.5f;
    bool attacked_ = false;
    Timer postHitTimer_;

    mc::EntitySnap* PickTarget(mc::WorldSnapshot& snap);
    Vec3 PickHitPoint(const mc::EntitySnap& e);
    void CalcAngles(const Vec3& from, const Vec3& to, float& yaw, float& pitch);
    void BeginReturn();
    void StepReturn(mc::WorldSnapshot& snap);
};

}  // namespace summer
