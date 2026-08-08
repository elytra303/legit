#pragma once

#include "../../module.h"
#include "../../util/timer.h"

namespace summer {

class ElytraSwapModule : public Module {
public:
    ElytraSwapModule();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    enum class Action { None, Equip, Unequip };
    enum class Stage { Idle, Delay, Cooldown };

    int mode_ = 1;  // 0 = swap key, 1 = double jump, 2 = auto (double jump + land)
    int swapKey_ = 0;
    bool humanize_ = true;
    int reactMin_ = 60, reactMax_ = 140;

    bool swapKeyPrev_ = false;
    bool wasJump_ = false;
    int jumps_ = 0;
    Timer jumpTimer_;
    bool falling_ = false;
    double prevY_ = 0.0;

    Action pending_ = Action::None;
    Stage stage_ = Stage::Idle;
    double delayMs_ = 0.0, cooldownMs_ = 0.0;
    Timer actionTimer_;

    double Jitter(double a, double b);
    bool IsChestElytra();
    bool IsChestplate(const std::string& name);
    int FindItem(const std::string& exactId);
    int FindChestplate();
    void Begin(Action a);
    void RunStage();
};

}  // namespace summer
