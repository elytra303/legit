#pragma once
#include <windows.h>
#include <atomic>

namespace cfg {

enum HitboxMode {
    HITBOX_OFF = 0,
    HITBOX_NORMAL = 1,
    HITBOX_LEGIT = 2
};

enum ModuleId {
    MOD_HITBOXES = 0,
    MOD_TRIGGERBOT,
    MOD_AUTOSWAP,
    MOD_ESP,
    MOD_REMOVALS,
    MOD_SPRINT,
    MOD_SCREENWALK,
    MOD_HUD,
    MOD_COUNT
};

struct Config {
    bool gui_open = false;
    int  menu_key = VK_INSERT;
    int  binding_target = -1;

    int   hitbox_mode = HITBOX_OFF;
    float hitbox_fov = 10.0f;
    float hitbox_range = 3.0f;
    bool  hitbox_random_point = true;
    bool  hitbox_render = true;
    float hitbox_delay_ms = 40.0f;

    bool  triggerbot = false;
    float trigger_range = 3.0f;
    float trigger_delay_ms = 130.0f;

    bool  autoswap = false;
    bool  autoswap_back = false;

    bool  esp = false;
    bool  esp_box = true;
    bool  esp_health = true;
    bool  esp_distance = false;
    bool  esp_tracers = false;

    bool  rem_fire = false;
    bool  rem_fog = false;
    bool  rem_nausea = false;

    bool  sprint = false;
    bool  screenwalk = false;

    bool  hud = true;
    bool  hud_fps = true;
    bool  hud_coords = true;
    bool  hud_direction = true;
    bool  hud_modules = true;

    int   keys[MOD_COUNT] = { VK_X, VK_F6, VK_F7, VK_V, VK_F8, VK_F9, VK_F10, VK_F11 };
};

extern Config g;
extern std::atomic<bool> fog_removed;

}
