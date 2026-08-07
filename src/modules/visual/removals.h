#pragma once

#include "../../module.h"

namespace summer {

class Removals : public Module {
public:
    Removals();

    void OnFrame() override;
    void OnDisable() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool fullbright_ = true;
    bool noHurtCam_ = true;
    bool noFire_ = true;
    bool noBob_ = false;
};

}  // namespace summer
