#pragma once

#include <jni.h>

#include <string>
#include <vector>

#include "../math/math.h"

namespace summer {
namespace mc {

// Snapshot of one entity. `ref` is a JNI local reference and is only valid
// during the frame in which the snapshot was captured.
struct EntitySnap {
    jobject ref = nullptr;
    bool isLocal = false;
    bool isPlayer = false;
    bool isAlive = false;
    bool isInvisible = false;
    bool hostile = false;
    bool allied = false;
    bool isMob = false;
    float health = 0.f;
    float maxHealth = 1.f;
    float yRot = 0.f, xRot = 0.f;
    int hurtTime = 0;
    int attackCooldownPct = 100;  // 0..100
    Vec3 pos;
    AABB box;
    std::string name;
    double dist = 0.0;
    double fovAngle = 360.0;  // angle between crosshair and entity center
};

struct WorldSnapshot {
    bool valid = false;
    bool hasScreen = false;
    Vec3 localPos;
    Vec3 eyePos;
    float localYaw = 0.f, localPitch = 0.f;
    float fov = 70.f;
    int viewportW = 0, viewportH = 0;
    std::vector<EntitySnap> entities;
    int localIndex = -1;
};

}  // namespace mc
}  // namespace summer
