#include "esp.h"

#include <cstdio>
#include <imgui.h>

#include "../../gui/gui.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

ESPModule::ESPModule()
    : Module("ESP", "Boxes, health and names on entities", Category::Visual) {}

void ESPModule::Save(Config& c) {
    c.SetBool("esp.enabled", Enabled());
    c.SetInt("esp.key", Key());
    c.SetBool("esp.players", players_);
    c.SetBool("esp.mobs", mobs_);
    c.SetBool("esp.invisible", invisible_);
    c.SetBool("esp.allies", showAllies_);
    c.SetDouble("esp.range", range_);
    c.SetBool("esp.box", box_);
    c.SetBool("esp.health", health_);
    c.SetBool("esp.name", name_);
    c.SetBool("esp.tracer", tracer_);
    c.SetBool("esp.distance", distance_);
    c.SetBool("esp.hitbox3d", hitbox3D_);
}

void ESPModule::Load(const Config& c) {
    SetEnabled(c.GetBool("esp.enabled", false));
    SetKey(c.GetInt("esp.key", 0));
    players_ = c.GetBool("esp.players", true);
    mobs_ = c.GetBool("esp.mobs", true);
    invisible_ = c.GetBool("esp.invisible", true);
    showAllies_ = c.GetBool("esp.allies", false);
    range_ = (float)c.GetDouble("esp.range", 256.0);
    box_ = c.GetBool("esp.box", true);
    health_ = c.GetBool("esp.health", true);
    name_ = c.GetBool("esp.name", true);
    tracer_ = c.GetBool("esp.tracer", false);
    distance_ = c.GetBool("esp.distance", false);
    hitbox3D_ = c.GetBool("esp.hitbox3d", false);
}

ImU32 ESPModule::ColorFor(const mc::EntitySnap& e) const {
    if (e.isPlayer) return e.allied ? IM_COL32(90, 220, 110, 255)
                                    : IM_COL32(240, 70, 70, 255);
    return e.hostile ? IM_COL32(255, 160, 40, 255) : IM_COL32(120, 140, 255, 255);
}

void ESPModule::ProjectBox(const CameraState& cam, const AABB& b, float out[8][2],
                           bool& allInFront) {
    allInFront = true;
    Vec3 corners[8] = {
        {b.minX, b.minY, b.minZ}, {b.maxX, b.minY, b.minZ},
        {b.maxX, b.minY, b.maxZ}, {b.minX, b.minY, b.maxZ},
        {b.minX, b.maxY, b.minZ}, {b.maxX, b.maxY, b.minZ},
        {b.maxX, b.maxY, b.maxZ}, {b.minX, b.maxY, b.maxZ},
    };
    for (int i = 0; i < 8; ++i) {
        bool behind;
        float sx, sy;
        WorldToScreen(cam, corners[i], sx, sy, behind);
        if (behind) allInFront = false;
        out[i][0] = sx;
        out[i][1] = sy;
    }
}

void ESPModule::OnRender() {
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
    ImFont* font = ImGui::GetFont();

    for (auto& e : snap.entities) {
        if (e.isLocal) continue;
        if (!e.isAlive) continue;
        if (invisible_ && e.isInvisible) continue;
        if (e.isPlayer) {
            if (!players_) continue;
            if (!showAllies_ && e.allied) continue;
        } else {
            if (!mobs_) continue;
        }
        if (e.dist > range_) continue;

        float corners[8][2];
        bool allInFront;
        ProjectBox(cam, e.box, corners, allInFront);
        if (!allInFront) continue;

        float minX = corners[0][0], minY = corners[0][1];
        float maxX = corners[0][0], maxY = corners[0][1];
        for (int i = 1; i < 8; ++i) {
            minX = std::min(minX, corners[i][0]);
            maxX = std::max(maxX, corners[i][0]);
            minY = std::min(minY, corners[i][1]);
            maxY = std::max(maxY, corners[i][1]);
        }

        ImU32 col = ColorFor(e);
        float bh = maxY - minY;

        if (box_) {
            dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, 1.2f);
            dl->AddRect(ImVec2(minX + 1, minY + 1), ImVec2(maxX - 1, maxY - 1),
                        IM_COL32(0, 0, 0, 120));
        }

        if (hitbox3D_) {
            static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                             {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                             {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (auto& ed : edges)
                dl->AddLine(ImVec2(corners[ed[0]][0], corners[ed[0]][1]),
                            ImVec2(corners[ed[1]][0], corners[ed[1]][1]), col, 1.2f);
        }

        float textY = minY;
        if (name_) {
            textY -= 14;
            dl->AddText(font, 13.f, ImVec2(minX, textY), col, e.name.c_str());
            textY -= 13;
        }
        if (distance_) {
            char buf[32];
            snprintf(buf, sizeof buf, "%.0fm", e.dist);
            dl->AddText(font, 12.f, ImVec2(minX, textY), IM_COL32(220, 220, 220, 220),
                        buf);
        }

        if (health_ && e.maxHealth > 0.f) {
            float frac = std::clamp(e.health / e.maxHealth, 0.f, 1.f);
            float h = bh;
            ImU32 hcol = ImLerp(IM_COL32(240, 60, 60, 255), IM_COL32(90, 230, 90, 255),
                                frac);
            float bx = minX - 4.f;
            dl->AddRectFilled(ImVec2(bx - 1, minY - 1), ImVec2(bx + 2, maxY + 1),
                              IM_COL32(0, 0, 0, 160));
            dl->AddRectFilled(ImVec2(bx, maxY - h * frac), ImVec2(bx + 1, maxY), hcol);
        }

        if (tracer_) {
            ImU32 tcol =
                IM_COL32(col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF, 90);
            dl->AddLine(ImVec2(cam.w * 0.5f, (float)cam.h),
                        ImVec2((minX + maxX) * 0.5f, minY), tcol, 1.f);
        }
    }
}

void ESPModule::DrawSettings() {
    gui::Section("FILTER");
    gui::Checkbox("Players", &players_);
    gui::Checkbox("Mobs", &mobs_);
    gui::Checkbox("Invisible", &invisible_);
    gui::Checkbox("Show teammates", &showAllies_);
    gui::SliderFloat("Range", &range_, 10.f, 512.f);
    gui::Separator();
    gui::Section("RENDER");
    gui::Checkbox("2D Box", &box_);
    gui::Checkbox("3D Hitbox", &hitbox3D_);
    gui::Checkbox("Health bar", &health_);
    gui::Checkbox("Name", &name_);
    gui::Checkbox("Distance", &distance_);
    gui::Checkbox("Tracers", &tracer_);
}

}  // namespace summer
