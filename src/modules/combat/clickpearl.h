#pragma once

#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class ClickPearlModule : public Module {
public:
    ClickPearlModule();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    int mode_ = 0;  // 0 = while holding use, 1 = auto loop
    bool humanize_ = true;
    int minDelay_ = 190, maxDelay_ = 260;
    int pressMin_ = 55, pressMax_ = 95;

    bool switching_ = false;
    bool clicking_ = false;
    Timer switchTimer_, clickTimer_;
    double switchDelay_ = 0.0;
    double nextClick_ = 0.0;
    double pressDelay_ = 0.0;

    int FindPearlSlot();
    double Jitter(double a, double b);
};

}  // namespace summer
