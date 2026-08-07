#pragma once

#include "../../mc/types.h"
#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class AutoSwap : public Module {
public:
    AutoSwap();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool switchBack_ = true;
    bool targetPlayers_ = true;
    bool targetMobs_ = false;
    bool ignoreTeammates_ = true;
    float range_ = 5.0f;

    bool inCombat_ = false;
    bool switching_ = false;
    Timer switchTimer_;
    double switchDelay_ = 60.0;
    int prevSlot_ = -1;
    int bestSlot_ = -1;
    Timer backTimer_;
    double backDelay_ = 150.0;
    bool backing_ = false;

    bool HasTargetNear(mc::WorldSnapshot& snap);
    int ScoreItem(const std::string& name);
};

}  // namespace summer
