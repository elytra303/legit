#include "modules.h"
#include "config.h"
#include "minecraft.h"
#include "geometry.h"
#include <windows.h>
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace modules {

static bool g_held[256] = {};
static bool g_edge[256] = {};

static bool key_held(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static void toggle_module(int id) {
    switch (id) {
    case cfg::MOD_HITBOXES:
        cfg::g.hitbox_mode = (cfg::g.hitbox_mode == cfg::HITBOX_OFF) ? cfg::HITBOX_LEGIT : cfg::HITBOX_OFF;
        break;
    case cfg::MOD_TRIGGERBOT:
        cfg::g.triggerbot = !cfg::g.triggerbot;
        break;
    case cfg::MOD_AUTOSWAP:
        cfg::g.autoswap = !cfg::g.autoswap;
        break;
    case cfg::MOD_ESP:
        cfg::g.esp = !cfg::g.esp;
        break;
    case cfg::MOD_REMOVALS: {
        bool any = cfg::g.rem_fire || cfg::g.rem_fog || cfg::g.rem_nausea;
        cfg::g.rem_fire = cfg::g.rem_fog = cfg::g.rem_nausea = !any;
        break;
    }
    case cfg::MOD_SPRINT:
        cfg::g.sprint = !cfg::g.sprint;
        break;
    case cfg::MOD_SCREENWALK:
        cfg::g.screenwalk = !cfg::g.screenwalk;
        break;
    case cfg::MOD_HUD:
        cfg::g.hud = !cfg::g.hud;
        break;
    }
}

void poll_keys() {
    for (int vk = 0x08; vk <= 0xFE; vk++) {
        bool down = key_held(vk);
        g_edge[vk] = down && !g_held[vk];
        g_held[vk] = down;
    }
    if (g_edge[VK_ESCAPE] && cfg::g.gui_open) {
        cfg::g.gui_open = false;
        return;
    }
    if (g_edge[cfg::g.menu_key])
        cfg::g.gui_open = !cfg::g.gui_open;
    if (cfg::g.binding_target >= 0) {
        for (int vk = 0x08; vk <= 0xFE; vk++) {
            if (g_edge[vk] && vk != cfg::g.menu_key && vk != VK_ESCAPE) {
                cfg::g.keys[cfg::g.binding_target] = vk;
                cfg::g.binding_target = -1;
                return;
            }
        }
    } else if (!cfg::g.gui_open) {
        for (int i = 0; i < cfg::MOD_COUNT; i++)
            if (g_edge[cfg::g.keys[i]])
                toggle_module(i);
    }
}

struct EntityInfo {
    jobject obj;
    Box3d box;
    Vec3d pos;
    float health;
};

static Box3d find_box_for(JNIEnv* e, jobject ent);

static bool read_entity(JNIEnv* e, jobject ent, EntityInfo& out) {
    jobject box = e->GetObjectField(ent, mc::B.f_ent_boundingBox);
    if (!box) {
        e->ExceptionClear();
        return false;
    }
    out.box = Box3d{
        e->GetDoubleField(box, mc::B.f_box_minX),
        e->GetDoubleField(box, mc::B.f_box_minY),
        e->GetDoubleField(box, mc::B.f_box_minZ),
        e->GetDoubleField(box, mc::B.f_box_maxX),
        e->GetDoubleField(box, mc::B.f_box_maxY),
        e->GetDoubleField(box, mc::B.f_box_maxZ)
    };
    e->DeleteLocalRef(box);
    out.pos.x = e->CallDoubleMethod(ent, mc::B.m_ent_getX);
    out.pos.y = e->CallDoubleMethod(ent, mc::B.m_ent_getY);
    out.pos.z = e->CallDoubleMethod(ent, mc::B.m_ent_getZ);
    if (e->ExceptionCheck()) {
        e->ExceptionClear();
        return false;
    }
    if (mc::B.m_liv_getHealth && e->IsInstanceOf(ent, mc::B.c_LivingEntity)) {
        out.health = e->CallFloatMethod(ent, mc::B.m_liv_getHealth);
        if (e->ExceptionCheck()) e->ExceptionClear();
    } else {
        out.health = 20.0f;
    }
    out.obj = ent;
    return true;
}

static std::vector<EntityInfo> collect_players(JNIEnv* e) {
    std::vector<EntityInfo> v;
    jobject w = mc::world(e);
    if (!w) return v;
    jobject list = e->GetObjectField(w, mc::B.f_world_players);
    if (list) {
        jint n = e->CallIntMethod(list, mc::B.m_list_size);
        for (jint i = 0; i < n; i++) {
            jobject ent = e->CallObjectMethod(list, mc::B.m_list_get, i);
            if (!ent) {
                e->ExceptionClear();
                continue;
            }
            jobject self = mc::player(e);
            if (self && e->IsSameObject(ent, self)) {
                e->DeleteLocalRef(ent);
                e->DeleteLocalRef(self);
                continue;
            }
            if (self) e->DeleteLocalRef(self);
            EntityInfo info;
            if (read_entity(e, ent, info))
                v.push_back(info);
            else
                e->DeleteLocalRef(ent);
        }
        e->DeleteLocalRef(list);
    }
    if (e->ExceptionCheck()) e->ExceptionClear();
    return v;
}

static Vec3d camera_pos(JNIEnv* e) {
    jobject cam = mc::camera(e);
    if (!cam) return {};
    jobject pos = e->GetObjectField(cam, mc::B.f_cam_pos);
    if (!pos) {
        e->ExceptionClear();
        e->DeleteLocalRef(cam);
        return {};
    }
    Vec3d p = {
        e->GetDoubleField(pos, mc::B.f_v3_x),
        e->GetDoubleField(pos, mc::B.f_v3_y),
        e->GetDoubleField(pos, mc::B.f_v3_z)
    };
    e->DeleteLocalRef(pos);
    e->DeleteLocalRef(cam);
    return p;
}

static Vec3d view_dir(JNIEnv* e) {
    jobject p = mc::player(e);
    if (!p) return {};
    float yaw = e->CallFloatMethod(p, mc::B.m_ent_getYaw);
    float pitch = e->CallFloatMethod(p, mc::B.m_ent_getPitch);
    e->DeleteLocalRef(p);
    return dir_from_yaw_pitch(yaw, pitch);
}

static int weapon_score(const char* key) {
    if (!key) return -1;
    bool sword = strstr(key, "_sword") != nullptr;
    bool axe = strstr(key, "_axe") != nullptr;
    if (!sword && !axe) return -1;
    int mat = 0;
    if (strstr(key, "netherite")) mat = 5;
    else if (strstr(key, "diamond")) mat = 4;
    else if (strstr(key, "iron")) mat = 3;
    else if (strstr(key, "stone")) mat = 2;
    else if (strstr(key, "wood")) mat = 1;
    return mat * 10 + (sword ? 5 : 2);
}

static int best_weapon_slot(JNIEnv* e, jobject inv) {
    int best = -1;
    int best_score = -1;
    jobject main = e->GetObjectField(inv, mc::B.f_inv_main);
    if (!main) return -1;
    for (int i = 0; i < 9; i++) {
        jobject stack = e->CallObjectMethod(main, mc::B.m_dl_get, i);
        if (!stack) {
            e->ExceptionClear();
            continue;
        }
        if (e->CallBooleanMethod(stack, mc::B.m_is_isEmpty)) {
            e->DeleteLocalRef(stack);
            continue;
        }
        jobject item = e->CallObjectMethod(stack, mc::B.m_is_getItem);
        if (!item) {
            e->ExceptionClear();
            e->DeleteLocalRef(stack);
            continue;
        }
        jstring key = (jstring)e->CallObjectMethod(item, mc::B.m_i_getTranslationKey);
        int score = -1;
        if (key) {
            const char* k = e->GetStringUTFChars(key, nullptr);
            score = weapon_score(k);
            if (k) e->ReleaseStringUTFChars(key, k);
        }
        if (key) e->DeleteLocalRef(key);
        e->DeleteLocalRef(item);
        e->DeleteLocalRef(stack);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    e->DeleteLocalRef(main);
    if (e->ExceptionCheck()) e->ExceptionClear();
    return best;
}

static bool attack_entity(JNIEnv* e, jobject target) {
    jobject player = mc::player(e);
    jobject im = mc::interaction_manager(e);
    if (!player || !im || !target) return false;
    int orig_slot = -1;
    if (cfg::g.autoswap) {
        jobject inv = e->GetObjectField(player, mc::B.f_pe_inventory);
        if (inv) {
            int cur = e->GetIntField(inv, mc::B.f_inv_selectedSlot);
            int best = best_weapon_slot(e, inv);
            if (best >= 0 && best != cur) {
                orig_slot = cur;
                e->CallVoidMethod(inv, mc::B.m_inv_setSelectedSlot, best);
            }
            e->DeleteLocalRef(inv);
        }
    }
    e->CallVoidMethod(im, mc::B.m_im_attackEntity, player, target);
    bool ok = !e->ExceptionCheck();
    if (!ok) e->ExceptionClear();
    if (cfg::g.autoswap && cfg::g.autoswap_back && orig_slot >= 0) {
        jobject inv = e->GetObjectField(player, mc::B.f_pe_inventory);
        if (inv) {
            e->CallVoidMethod(inv, mc::B.m_inv_setSelectedSlot, orig_slot);
            e->DeleteLocalRef(inv);
        }
    }
    if (player) e->DeleteLocalRef(player);
    if (im) e->DeleteLocalRef(im);
    return ok;
}

static double g_last_trigger = 0.0;

static void triggerbot(JNIEnv* e, const Vec3d& eye, const Vec3d& dir) {
    if (!cfg::g.triggerbot) return;
    double now = (double)GetTickCount64();
    double delay = cfg::g.trigger_delay_ms + (double)(rand() % 40);
    if (now - g_last_trigger < delay) return;
    std::vector<EntityInfo> players = collect_players(e);
    for (size_t i = 0; i < players.size(); i++) {
        double t = 0.0;
        if (ray_aabb(eye, dir, players[i].box, cfg::g.trigger_range, &t)) {
            attack_entity(e, players[i].obj);
            g_last_trigger = now;
            break;
        }
    }
}

struct PendingHit {
    jobject ref = nullptr;
    Box3d box;
    float yaw0 = 0.0f;
    float pitch0 = 0.0f;
    double fire_at = 0.0;
    bool active = false;
};

static PendingHit g_pending;

static void release_pending(JNIEnv* e) {
    if (g_pending.ref) {
        e->DeleteGlobalRef(g_pending.ref);
        g_pending.ref = nullptr;
    }
    g_pending.active = false;
}

static void hitboxes(JNIEnv* e, const Vec3d& eye, const Vec3d& dir) {
    if (cfg::g.hitbox_mode == cfg::HITBOX_OFF) {
        if (g_pending.active) release_pending(e);
        return;
    }
    if (cfg::g.hitbox_mode == cfg::HITBOX_NORMAL) return;

    if (g_pending.active) {
        double now = (double)GetTickCount64();
        if (now >= g_pending.fire_at) {
            jobject target = g_pending.ref;
            g_pending.active = false;
            g_pending.ref = nullptr;
            jobject player = mc::player(e);
            if (player && target) {
                Vec3d aim = {
                    g_pending.box.minX + (g_pending.box.maxX - g_pending.box.minX) * ((cfg::g.hitbox_random_point) ? (rand() % 1000) / 1000.0 : 0.5),
                    g_pending.box.minY + (g_pending.box.maxY - g_pending.box.minY) * ((cfg::g.hitbox_random_point) ? (rand() % 1000) / 1000.0 : 0.35),
                    g_pending.box.minZ + (g_pending.box.maxZ - g_pending.box.minZ) * ((cfg::g.hitbox_random_point) ? (rand() % 1000) / 1000.0 : 0.5)
                };
                float yaw = 0.0f, pitch = 0.0f;
                angles_to(eye, aim, yaw, pitch);
                float jitter = ((rand() % 120) - 60) / 100.0f;
                e->CallVoidMethod(player, mc::B.m_ent_setYaw, yaw + jitter);
                e->CallVoidMethod(player, mc::B.m_ent_setPitch, pitch + jitter);
                attack_entity(e, target);
                e->CallVoidMethod(player, mc::B.m_ent_setYaw, g_pending.yaw0);
                e->CallVoidMethod(player, mc::B.m_ent_setPitch, g_pending.pitch0);
            }
            if (player) e->DeleteLocalRef(player);
            e->DeleteGlobalRef(target);
        }
        return;
    }

    if (!key_held(VK_LBUTTON)) return;

    std::vector<EntityInfo> players = collect_players(e);
    for (size_t i = 0; i < players.size(); i++) {
        double t = 0.0;
        if (ray_aabb(eye, dir, players[i].box, cfg::g.hitbox_range, &t)) {
            attack_entity(e, players[i].obj);
            return;
        }
    }

    jobject best = nullptr;
    double best_angle = radians(cfg::g.hitbox_fov);
    for (size_t i = 0; i < players.size(); i++) {
        Vec3d center = {
            (players[i].box.minX + players[i].box.maxX) * 0.5,
            (players[i].box.minY + players[i].box.maxY) * 0.5,
            (players[i].box.minZ + players[i].box.maxZ) * 0.5
        };
        Vec3d to = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
        double dl = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
        if (dl > cfg::g.hitbox_range || dl < 1e-6) continue;
        double dot = (dir.x * to.x + dir.y * to.y + dir.z * to.z) / dl;
        if (dot > 1.0) dot = 1.0;
        if (dot < -1.0) dot = -1.0;
        double ang = std::acos(dot);
        if (ang < best_angle) {
            best_angle = ang;
            best = players[i].obj;
        }
    }
    if (!best) return;

    jobject player = mc::player(e);
    if (!player) return;
    release_pending(e);
    g_pending.ref = e->NewGlobalRef(best);
    g_pending.box = find_box_for(e, best);
    g_pending.yaw0 = e->CallFloatMethod(player, mc::B.m_ent_getYaw);
    g_pending.pitch0 = e->CallFloatMethod(player, mc::B.m_ent_getPitch);
    double delay = cfg::g.hitbox_delay_ms * 0.5 + (rand() % (int)(cfg::g.hitbox_delay_ms + 1));
    g_pending.fire_at = (double)GetTickCount64() + delay;
    g_pending.active = true;
    e->DeleteLocalRef(player);
}

static void sprint(JNIEnv* e, bool screen_open) {
    static bool was_sprinting = false;
    jobject player = mc::player(e);
    if (!player || !mc::B.m_ent_setSprinting) return;
    bool want = false;
    if (cfg::g.sprint && !screen_open) {
        bool fwd = key_held(0x57);
        bool sneak = key_held(VK_SHIFT);
        want = fwd && !sneak;
    }
    if (want != was_sprinting) {
        e->CallVoidMethod(player, mc::B.m_ent_setSprinting, want);
        was_sprinting = want;
    }
    e->DeleteLocalRef(player);
}

static void screenwalk(JNIEnv* e, bool screen_open) {
    if (!cfg::g.screenwalk || !screen_open || !mc::B.m_pi_init6) return;
    jobject input = mc::input(e);
    if (!input) return;
    bool fwd = key_held(0x57);
    bool back = key_held(0x53);
    bool left = key_held(0x41);
    bool right = key_held(0x44);
    bool sneak = key_held(VK_SHIFT);
    bool run = key_held(VK_CONTROL);
    jobject pi = e->NewObject(mc::B.c_PlayerInput, mc::B.m_pi_init6,
                              fwd, back, left, right, sneak, run);
    if (pi) {
        e->SetObjectField(input, mc::B.f_in_playerInput, pi);
        e->DeleteLocalRef(pi);
    } else {
        e->ExceptionClear();
    }
    jobject vec = e->NewObject(mc::B.c_Vec2f, mc::B.m_v2_init,
                               (float)((right ? 1 : 0) - (left ? 1 : 0)),
                               (float)((fwd ? 1 : 0) - (back ? 1 : 0)));
    if (vec) {
        e->SetObjectField(input, mc::B.f_in_movementVector, vec);
        e->DeleteLocalRef(vec);
    } else {
        e->ExceptionClear();
    }
    if (key_held(VK_SPACE) && mc::B.m_in_jump)
        e->CallVoidMethod(input, mc::B.m_in_jump);
    e->DeleteLocalRef(input);
    if (e->ExceptionCheck()) e->ExceptionClear();
}

static void removals(JNIEnv* e) {
    jobject player = mc::player(e);
    if (!player) return;
    if (cfg::g.rem_fire && mc::B.m_ent_setFireTicks)
        e->CallVoidMethod(player, mc::B.m_ent_setFireTicks, 0);
    if (cfg::g.rem_nausea && mc::B.f_cpe_nauseaIntensity) {
        e->SetFloatField(player, mc::B.f_cpe_nauseaIntensity, 0.0f);
        if (mc::B.f_cpe_lastNauseaIntensity)
            e->SetFloatField(player, mc::B.f_cpe_lastNauseaIntensity, 0.0f);
    }
    e->DeleteLocalRef(player);
    if (e->ExceptionCheck()) e->ExceptionClear();
}

static float esp_fov(JNIEnv* e) {
    if (mc::B.m_gr_getFov) {
        jobject gr = mc::game_renderer(e);
        jobject cam = mc::camera(e);
        if (gr && cam) {
            float f = e->CallFloatMethod(gr, mc::B.m_gr_getFov, cam, 1.0f, false);
            if (!e->ExceptionCheck() && f > 20.0f && f < 130.0f) {
                e->DeleteLocalRef(gr);
                e->DeleteLocalRef(cam);
                return f;
            }
            e->ExceptionClear();
        }
        if (gr) e->DeleteLocalRef(gr);
        if (cam) e->DeleteLocalRef(cam);
    }
    return 70.0f;
}

static void draw_box3d(ImDrawList* dl, const Vec3d& cam_pos, double yaw, double pitch,
                       double fov, int sw, int sh, const Box3d& b, ImU32 color) {
    double cx[8], cy[8];
    bool ok = true;
    const double xs[8] = { b.minX, b.maxX, b.maxX, b.minX, b.minX, b.maxX, b.maxX, b.minX };
    const double ys[8] = { b.minY, b.minY, b.minY, b.minY, b.maxY, b.maxY, b.maxY, b.maxY };
    const double zs[8] = { b.minZ, b.minZ, b.maxZ, b.maxZ, b.minZ, b.minZ, b.maxZ, b.maxZ };
    for (int i = 0; i < 8; i++) {
        if (!world_to_screen(cam_pos, yaw, pitch, fov, sw, sh, { xs[i], ys[i], zs[i] }, cx[i], cy[i])) {
            ok = false;
            break;
        }
    }
    if (!ok) return;
    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    for (int i = 0; i < 12; i++) {
        int a = edges[i][0], b2 = edges[i][1];
        dl->AddLine(ImVec2((float)cx[a], (float)cy[a]), ImVec2((float)cx[b2], (float)cy[b2]), color);
    }
}

static void esp(JNIEnv* e) {
    bool want_esp = cfg::g.esp;
    bool want_hitbox = cfg::g.hitbox_mode != cfg::HITBOX_OFF && cfg::g.hitbox_render;
    if (!want_esp && !want_hitbox) return;

    jobject cam = mc::camera(e);
    if (!cam) return;
    Vec3d cpos = camera_pos(e);
    float yaw = e->CallFloatMethod(cam, mc::B.m_cam_getYaw);
    float pitch = e->CallFloatMethod(cam, mc::B.m_cam_getPitch);
    float fov = esp_fov(e);
    e->DeleteLocalRef(cam);

    ImGuiIO& io = ImGui::GetIO();
    int sw = (int)io.DisplaySize.x;
    int sh = (int)io.DisplaySize.y;
    if (sw <= 0 || sh <= 0) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    std::vector<EntityInfo> players = collect_players(e);
    for (size_t i = 0; i < players.size(); i++) {
        const EntityInfo& p = players[i];

        if (want_hitbox) {
            ImU32 hb = IM_COL32(255, 255, 255, 190);
            if (g_pending.active && g_pending.ref && e->IsSameObject(g_pending.ref, p.obj))
                hb = IM_COL32(80, 255, 80, 230);
            draw_box3d(dl, cpos, yaw, pitch, fov, sw, sh, p.box, hb);
        }

        if (!want_esp) continue;

        double sx[8], sy[8];
        const double xs[8] = { p.box.minX, p.box.maxX, p.box.maxX, p.box.minX, p.box.minX, p.box.maxX, p.box.maxX, p.box.minX };
        const double ys[8] = { p.box.minY, p.box.minY, p.box.minY, p.box.minY, p.box.maxY, p.box.maxY, p.box.maxY, p.box.maxY };
        const double zs[8] = { p.box.minZ, p.box.minZ, p.box.maxZ, p.box.maxZ, p.box.minZ, p.box.minZ, p.box.maxZ, p.box.maxZ };
        bool visible = true;
        double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
        for (int k = 0; k < 8; k++) {
            if (!world_to_screen(cpos, yaw, pitch, fov, sw, sh, { xs[k], ys[k], zs[k] }, sx[k], sy[k])) {
                visible = false;
                break;
            }
            if (sx[k] < minx) minx = sx[k];
            if (sx[k] > maxx) maxx = sx[k];
            if (sy[k] < miny) miny = sy[k];
            if (sy[k] > maxy) maxy = sy[k];
        }
        if (!visible) continue;

        if (cfg::g.esp_box) {
            float hp = p.health / 20.0f;
            if (hp < 0.0f) hp = 0.0f;
            if (hp > 1.0f) hp = 1.0f;
            ImU32 col = IM_COL32((int)(255 * (1.0f - hp)), (int)(255 * hp), 60, 255);
            dl->AddRect(ImVec2((float)minx, (float)miny), ImVec2((float)maxx, (float)maxy), col);
        }

        if (cfg::g.esp_tracers) {
            dl->AddLine(ImVec2(sw * 0.5f, (float)sh), ImVec2((float)((minx + maxx) * 0.5), (float)maxy),
                        IM_COL32(46, 133, 199, 200));
        }

        float text_y = (float)miny - 12.0f;
        if (text_y < 0.0f) text_y = 0.0f;
        char buf[64];
        if (cfg::g.esp_health && mc::B.m_liv_getHealth) {
            sprintf(buf, "%.0f HP", p.health);
            dl->AddText(ImVec2((float)minx, text_y), IM_COL32(120, 255, 120, 255), buf);
            text_y += 11.0f;
        }
        if (cfg::g.esp_distance) {
            double dx = p.pos.x - cpos.x, dy = p.pos.y - cpos.y, dz = p.pos.z - cpos.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            sprintf(buf, "%.1fm", dist);
            dl->AddText(ImVec2((float)minx, text_y), IM_COL32(200, 200, 200, 255), buf);
        }
    }
}

static const char* dir_name(float yaw) {
    float d = yaw;
    while (d < 0.0f) d += 360.0f;
    while (d >= 360.0f) d -= 360.0f;
    static const char* dirs[8] = { "S", "SW", "W", "NW", "N", "NE", "E", "SE" };
    int idx = (int)((d + 22.5f) / 45.0f) % 8;
    return dirs[idx];
}

static void hud(JNIEnv* e) {
    if (!cfg::g.hud) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float y = 6.0f;
    char buf[128];

    dl->AddText(ImVec2(8.0f, y), IM_COL32(46, 133, 199, 255), "SUMMER CLIENT");
    y += 16.0f;

    jobject player = mc::player(e);
    if (!player) return;

    if (cfg::g.hud_fps && mc::B.m_getCurrentFps) {
        jobject c = mc::mc_client(e);
        if (c) {
            int fps = e->CallIntMethod(c, mc::B.m_getCurrentFps);
            sprintf(buf, "FPS: %d", fps);
            dl->AddText(ImVec2(8.0f, y), IM_COL32(255, 255, 255, 255), buf);
            y += 12.0f;
            e->DeleteLocalRef(c);
        }
    }

    if (cfg::g.hud_coords) {
        double x = e->CallDoubleMethod(player, mc::B.m_ent_getX);
        double yy = e->CallDoubleMethod(player, mc::B.m_ent_getY);
        double z = e->CallDoubleMethod(player, mc::B.m_ent_getZ);
        sprintf(buf, "XYZ: %.1f %.1f %.1f", x, yy, z);
        dl->AddText(ImVec2(8.0f, y), IM_COL32(255, 255, 255, 255), buf);
        y += 12.0f;
    }

    if (cfg::g.hud_direction) {
        float yaw = e->CallFloatMethod(player, mc::B.m_ent_getYaw);
        sprintf(buf, "Facing: %s", dir_name(yaw));
        dl->AddText(ImVec2(8.0f, y), IM_COL32(255, 255, 255, 255), buf);
        y += 12.0f;
    }

    if (cfg::g.hud_modules) {
        struct Named {
            const char* name;
            bool on;
        };
        Named mods[cfg::MOD_COUNT] = {
            { "Hitboxes", cfg::g.hitbox_mode != cfg::HITBOX_OFF },
            { "TriggerBot", cfg::g.triggerbot },
            { "AutoSwap", cfg::g.autoswap },
            { "ESP", cfg::g.esp },
            { "Removals", cfg::g.rem_fire || cfg::g.rem_fog || cfg::g.rem_nausea },
            { "Sprint", cfg::g.sprint },
            { "ScreenWalk", cfg::g.screenwalk },
            { "HUD", cfg::g.hud },
        };
        for (int i = 0; i < cfg::MOD_COUNT; i++) {
            if (!mods[i].on) continue;
            sprintf(buf, "%s", mods[i].name);
            dl->AddText(ImVec2(8.0f, y), IM_COL32(120, 255, 120, 255), buf);
            y += 12.0f;
        }
    }

    e->DeleteLocalRef(player);
}

static Box3d find_box_for(JNIEnv* e, jobject ent) {
    Box3d b = {};
    jobject box = e->GetObjectField(ent, mc::B.f_ent_boundingBox);
    if (!box) {
        e->ExceptionClear();
        return b;
    }
    b = Box3d{
        e->GetDoubleField(box, mc::B.f_box_minX),
        e->GetDoubleField(box, mc::B.f_box_minY),
        e->GetDoubleField(box, mc::B.f_box_minZ),
        e->GetDoubleField(box, mc::B.f_box_maxX),
        e->GetDoubleField(box, mc::B.f_box_maxY),
        e->GetDoubleField(box, mc::B.f_box_maxZ)
    };
    e->DeleteLocalRef(box);
    return b;
}

void tick() {
    JNIEnv* e = mc::env();
    if (!e || !mc::bindings_ready) return;

    jobject player = mc::player(e);
    if (!player) {
        release_pending(e);
        return;
    }
    e->DeleteLocalRef(player);

    bool screen_open = mc::screen_open(e);
    bool in_world = mc::world(e) != nullptr;

    cfg::fog_removed.store(cfg::g.rem_fog);

    if (!in_world) return;

    Vec3d eye = camera_pos(e);
    Vec3d dir = view_dir(e);

    removals(e);
    sprint(e, screen_open);
    screenwalk(e, screen_open);

    if (!screen_open) {
        triggerbot(e, eye, dir);
        hitboxes(e, eye, dir);
    }

    esp(e);
    hud(e);
}

}
