#pragma once

#include "../../math/math.h"
#include "../../module.h"

namespace summer {

class HitboxesRender : public Module {
public:
    HitboxesRender();

    void OnRender() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool players_ = true;
    bool mobs_ = true;
    bool allies_ = false;
    float range_ = 64.f;
    bool outline_ = true;
};

}  // namespace summer
