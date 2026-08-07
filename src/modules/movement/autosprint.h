#pragma once

#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class AutoSprint : public Module {
public:
    AutoSprint();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool onlyForward_ = true;
    bool noSneak_ = true;
    bool noFoodCheck_ = false;
    int engageMinMs_ = 0;
    int engageMaxMs_ = 250;

    bool engaged_ = false;
    Timer engageTimer_;
    double engageDelay_ = 0.0;
};

}  // namespace summer
