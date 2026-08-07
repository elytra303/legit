#include "hitboxes.h"

#include <imgui.h>

#include "../../gui/gui.h"
#include "../../client.h"

namespace summer {

HitboxesRender::HitboxesRender()
    : Module("Hitboxes", "Draws the real hitbox of every entity in 3D",
             Category::Visual) {}

void HitboxesRender::Save(Config& c) {
    c.SetBool("hitboxes.enabled", Enabled());
    c.SetInt("hitboxes.key", Key());
    c.SetBool("hitboxes.players", players_);
    c.SetBool("hitboxes.mobs", mobs_);
    c.SetBool("hitboxes.allies", allies_);
    c.SetDouble("hitboxes.range", range_);
    c.SetBool("hitboxes.outline", outline_);
}

void HitboxesRender::Load(const Config& c) {
    SetEnabled(c.GetBool("hitboxes.enabled", false));
    SetKey(c.GetInt("hitboxes.key", 0));
    players_ = c.GetBool("hitboxes.players", true);
    mobs_ = c.GetBool("hitboxes.mobs", true);
    allies_ = c.GetBool("hitboxes.allies", false);
    range_ = (float)c.GetDouble("hitboxes.range", 64.0);
    outline_ = c.GetBool("hitboxes.outline", true);
}

void HitboxesRender::OnRender() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) return;

    CameraState cam;
    cam.pos = snap.eyePos;
    cam.yaw = snap.localYaw;
    cam.pitch = snap.localPitch;
    cam.fov = snap.fov;
    cam.w = snap.viewportW;
    cam.h = snap.viewportH;
    if (cam.w <= 0 || cam.h <= 0) return;

    ImDrawList* dl = gui::WorldDrawList();

    static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                     {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                     {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    for (auto& e : snap.entities) {
        if (e.isLocal || !e.isAlive) continue;
        if (e.dist > range_) continue;
        if (e.isPlayer) {
            if (!players_) continue;
            if (!allies_ && e.allied) continue;
        } else {
            if (!mobs_) continue;
        }

        Vec3 corners[8] = {
            {e.box.minX, e.box.minY, e.box.minZ},
            {e.box.maxX, e.box.minY, e.box.minZ},
            {e.box.maxX, e.box.minY, e.box.maxZ},
            {e.box.minX, e.box.minY, e.box.maxZ},
            {e.box.minX, e.box.maxY, e.box.minZ},
            {e.box.maxX, e.box.maxY, e.box.minZ},
            {e.box.maxX, e.box.maxY, e.box.maxZ},
            {e.box.minX, e.box.maxY, e.box.maxZ},
        };
        float sx[8], sy[8];
        bool allFront = true;
        for (int i = 0; i < 8; ++i) {
            bool behind;
            WorldToScreen(cam, corners[i], sx[i], sy[i], behind);
            if (behind) allFront = false;
        }
        if (!allFront) continue;

        ImU32 col = e.isPlayer ? (e.allied ? IM_COL32(90, 220, 110, 255)
                                           : IM_COL32(240, 70, 70, 255))
                               : (e.hostile ? IM_COL32(255, 160, 40, 255)
                                            : IM_COL32(120, 140, 255, 255));
        for (auto& ed : edges) {
            if (outline_)
                dl->AddLine(ImVec2(sx[ed[0]] + 1, sy[ed[0]] + 1),
                            ImVec2(sx[ed[1]] + 1, sy[ed[1]] + 1),
                            IM_COL32(0, 0, 0, 160), 2.2f);
            dl->AddLine(ImVec2(sx[ed[0]], sy[ed[0]]), ImVec2(sx[ed[1]], sy[ed[1]]),
                        col, 1.4f);
        }
    }
}

void HitboxesRender::DrawSettings() {
    gui::Checkbox("Players", &players_);
    gui::Checkbox("Mobs", &mobs_);
    gui::Checkbox("Show allies", &allies_);
    gui::SliderFloat("Range", &range_, 10.f, 128.f);
    gui::Checkbox("Outline", &outline_);
}

}  // namespace summer
