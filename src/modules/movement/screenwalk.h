#pragma once

#include "../../module.h"

namespace summer {

class ScreenWalk : public Module {
public:
    ScreenWalk();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool forward_ = true;
    bool strafe_ = true;
    bool jump_ = false;
};

}  // namespace summer
