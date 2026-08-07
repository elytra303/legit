#pragma once

#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class TriggerBot : public Module {
public:
    TriggerBot();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool targetPlayers_ = true;
    bool targetMobs_ = false;
    bool ignoreTeammates_ = true;
    bool ignoreInvisible_ = true;
    bool requireKey_ = false;
    float range_ = 4.0f;
    float fov_ = 4.0f;
    int delayMinMs_ = 40;
    int delayMaxMs_ = 140;

    Timer scanTimer_;
    bool waiting_ = false;
    Timer fireTimer_;
    double fireDelay_ = 60.0;
    Timer afterTimer_;
    double afterDelay_ = 350.0;

    mc::EntitySnap* RaycastTarget(mc::WorldSnapshot& snap);
};

}  // namespace summer
