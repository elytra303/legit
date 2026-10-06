#pragma once

#include "../../module.h"

namespace summer {

// Enlarges entity hitboxes client-side. A JVMTI hook on Entity.getBoundingBox
// expands the box at read time so combat raycasts actually see it (writing
// from the render loop is too late - the game tick resets AABBs first).
class HitBoxModule : public Module {
public:
    HitBoxModule();

    void OnFrame() override;
    void OnEnable() override;
    void OnDisable() override;
    void DrawSettings() override;
    void Save(Config& cfg) override;
    void Load(const Config& cfg) override;

private:
    float expand_ = 0.3f;   // blocks added on each horizontal side
    float expandY_ = 0.1f;  // blocks added on top and bottom
    bool players_ = true;
    bool mobs_ = true;
    bool ignoreTeammates_ = false;
    bool ignoreInvisible_ = false;
    bool reachOnly_ = false;      // only boxes within vanilla reach (3.0)
    bool crosshairOnly_ = false;  // only what the crosshair is roughly on
    bool jitter_ = false;
    float range_ = 4.f;
    float fov_ = 10.f;
};

}  // namespace summer
