#pragma once

#include <imgui.h>

#include "../../math/math.h"
#include "../../mc/types.h"
#include "../../module.h"

namespace summer {

class ESPModule : public Module {
public:
    ESPModule();

    void OnRender() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    bool players_ = true;
    bool mobs_ = true;
    bool invisible_ = true;
    bool showAllies_ = false;
    float range_ = 256.f;

    bool box_ = true;
    bool health_ = true;
    bool name_ = true;
    bool tracer_ = false;
    bool distance_ = false;
    bool hitbox3D_ = false;

    ImU32 ColorFor(const mc::EntitySnap& e) const;
    static void ProjectBox(const CameraState& cam, const AABB& b, float out[8][2],
                           bool& allInFront);
};

}  // namespace summer
