#pragma once

#include "../../module.h"

namespace summer {

// Enlarges entity hitboxes client-side so the crosshair has more room to
// connect. Boxes are recomputed from the entity's natural size every frame
// and only applied to what the player can already reach, so nothing looks
// off to the server.
class HitBoxModule : public Module {
public:
    HitBoxModule();

    void OnFrame() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    float expand_ = 0.25f;   // blocks added on each horizontal side
    float expandY_ = 0.1f;   // blocks added on top and bottom
    bool players_ = true;
    bool mobs_ = true;
    bool ignoreTeammates_ = true;
    bool ignoreInvisible_ = true;
    bool reachOnly_ = true;      // only boxes within vanilla reach (3.0)
    bool crosshairOnly_ = true;  // only what the crosshair is roughly on
    bool jitter_ = true;         // small random variation per entity
    float range_ = 4.f;
    float fov_ = 10.f;
};

}  // namespace summer
