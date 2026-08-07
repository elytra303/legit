#pragma once
#include <jni.h>

namespace mc {

struct Bindings {
    jclass c_MinecraftClient, c_GameRenderer, c_Camera, c_Entity, c_LivingEntity,
           c_PlayerEntity, c_ClientPlayerEntity, c_Input, c_PlayerInput, c_Vec2f,
           c_Vec3d, c_Box, c_InteractionManager, c_PlayerInventory, c_DefaultedList,
           c_ItemStack, c_Item, c_ClientWorld, c_List;

    jmethodID m_getInstance;
    jfieldID  f_player, f_world, f_currentScreen, f_gameRenderer, f_interactionManager;

    jfieldID  f_gr_camera;
    jmethodID m_gr_getFov;

    jfieldID  f_cam_pos;
    jmethodID m_cam_getYaw, m_cam_getPitch;

    jfieldID  f_ent_boundingBox;
    jmethodID m_ent_getYaw, m_ent_getPitch, m_ent_setYaw, m_ent_setPitch,
              m_ent_setSprinting, m_ent_setFireTicks, m_ent_getX, m_ent_getY, m_ent_getZ;

    jfieldID  f_cpe_input, f_cpe_nauseaIntensity, f_cpe_lastNauseaIntensity;

    jfieldID  f_in_playerInput, f_in_movementVector;
    jmethodID m_in_jump;

    jmethodID m_pi_init6;

    jmethodID m_v2_init;

    jfieldID  f_v3_x, f_v3_y, f_v3_z;

    jfieldID  f_box_minX, f_box_minY, f_box_minZ, f_box_maxX, f_box_maxY, f_box_maxZ;

    jmethodID m_im_attackEntity;

    jfieldID  f_pe_inventory;
    jfieldID  f_inv_selectedSlot, f_inv_main;
    jmethodID m_inv_setSelectedSlot;
    jmethodID m_dl_get;

    jmethodID m_is_getItem, m_is_isEmpty;
    jmethodID m_i_getTranslationKey;

    jfieldID  f_world_players;
    jmethodID m_list_size, m_list_get;

    jmethodID m_getCurrentFps, m_stop;
    jmethodID m_liv_getHealth;
};

extern Bindings B;
extern bool bindings_ready;

bool attach(JavaVM* vm);
JNIEnv* env();
bool init(JNIEnv* e);
void shutdown();

jobject mc_client(JNIEnv* e);
jobject player(JNIEnv* e);
jobject world(JNIEnv* e);
jobject camera(JNIEnv* e);
jobject input(JNIEnv* e);
jobject interaction_manager(JNIEnv* e);
jobject game_renderer(JNIEnv* e);
bool in_game(JNIEnv* e);
bool screen_open(JNIEnv* e);

}
