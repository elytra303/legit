#include "minecraft.h"
#include <jni.h>

static JavaVM* g_vm = nullptr;
static bool g_attached = false;

namespace mc {

Bindings B = {};
bool bindings_ready = false;

#define MC "net/minecraft/"

static bool find_class(JNIEnv* e, const char* name, jclass& out) {
    jclass c = e->FindClass(name);
    if (!c) {
        e->ExceptionClear();
        return false;
    }
    out = (jclass)e->NewGlobalRef(c);
    e->DeleteLocalRef(c);
    return true;
}

#define GET_METHOD(cls, field, name, sig) \
    B.field = e->GetMethodID(cls, name, sig); \
    if (!B.field) { e->ExceptionClear(); return false; }
#define GET_METHOD_OPT(cls, field, name, sig) \
    B.field = e->GetMethodID(cls, name, sig); \
    if (!B.field) { e->ExceptionClear(); }
#define GET_STATIC_METHOD(cls, field, name, sig) \
    B.field = e->GetStaticMethodID(cls, name, sig); \
    if (!B.field) { e->ExceptionClear(); return false; }
#define GET_FIELD(cls, field, name, sig) \
    B.field = e->GetFieldID(cls, name, sig); \
    if (!B.field) { e->ExceptionClear(); return false; }
#define GET_FIELD_OPT(cls, field, name, sig) \
    B.field = e->GetFieldID(cls, name, sig); \
    if (!B.field) { e->ExceptionClear(); }

bool init(JNIEnv* e) {
    if (bindings_ready) return true;

    if (!find_class(e, MC "class_310", B.c_MinecraftClient)) return false;
    if (!find_class(e, MC "class_746", B.c_ClientPlayerEntity)) return false;
    if (!find_class(e, MC "class_638", B.c_ClientWorld)) return false;
    if (!find_class(e, MC "class_744", B.c_Input)) return false;
    if (!find_class(e, MC "class_10185", B.c_PlayerInput)) return false;
    if (!find_class(e, MC "class_241", B.c_Vec2f)) return false;
    if (!find_class(e, MC "class_636", B.c_InteractionManager)) return false;
    if (!find_class(e, MC "class_757", B.c_GameRenderer)) return false;
    if (!find_class(e, MC "class_4184", B.c_Camera)) return false;
    if (!find_class(e, MC "class_1297", B.c_Entity)) return false;
    if (!find_class(e, MC "class_1309", B.c_LivingEntity)) return false;
    if (!find_class(e, MC "class_1657", B.c_PlayerEntity)) return false;
    if (!find_class(e, MC "class_1661", B.c_PlayerInventory)) return false;
    if (!find_class(e, MC "class_2371", B.c_DefaultedList)) return false;
    if (!find_class(e, MC "class_1799", B.c_ItemStack)) return false;
    if (!find_class(e, MC "class_1792", B.c_Item)) return false;
    if (!find_class(e, MC "class_243", B.c_Vec3d)) return false;
    if (!find_class(e, MC "class_238", B.c_Box)) return false;
    if (!find_class(e, "java/util/List", B.c_List)) return false;

    GET_STATIC_METHOD(B.c_MinecraftClient, m_getInstance, "method_1551", "()L" MC "class_310;");
    GET_METHOD(B.c_MinecraftClient, m_getCurrentFps, "method_47599", "()I");
    GET_METHOD(B.c_MinecraftClient, m_stop, "method_1490", "()V");

    GET_FIELD(B.c_MinecraftClient, f_player, "field_1724", "L" MC "class_746;");
    GET_FIELD(B.c_MinecraftClient, f_world, "field_1687", "L" MC "class_638;");
    GET_FIELD_OPT(B.c_MinecraftClient, f_currentScreen, "field_1755", "L" MC "class_437;");
    GET_FIELD(B.c_MinecraftClient, f_gameRenderer, "field_1773", "L" MC "class_757;");
    GET_FIELD(B.c_MinecraftClient, f_interactionManager, "field_1761", "L" MC "class_636;");

    GET_FIELD(B.c_GameRenderer, f_gr_camera, "field_18765", "L" MC "class_4184;");
    GET_METHOD_OPT(B.c_GameRenderer, m_gr_getFov, "method_19195", "(L" MC "class_4184;FZ)F");

    GET_FIELD(B.c_Camera, f_cam_pos, "field_18712", "L" MC "class_243;");
    GET_METHOD(B.c_Camera, m_cam_getYaw, "method_19330", "()F");
    GET_METHOD(B.c_Camera, m_cam_getPitch, "method_19329", "()F");

    GET_FIELD(B.c_Entity, f_ent_boundingBox, "field_6005", "L" MC "class_238;");
    GET_METHOD(B.c_Entity, m_ent_getYaw, "method_36454", "()F");
    GET_METHOD(B.c_Entity, m_ent_getPitch, "method_36455", "()F");
    GET_METHOD(B.c_Entity, m_ent_setYaw, "method_36456", "(F)V");
    GET_METHOD(B.c_Entity, m_ent_setPitch, "method_36457", "(F)V");
    GET_METHOD_OPT(B.c_Entity, m_ent_setSprinting, "method_5728", "(Z)V");
    GET_METHOD_OPT(B.c_Entity, m_ent_setFireTicks, "method_20803", "(I)V");
    GET_METHOD(B.c_Entity, m_ent_getX, "method_23317", "()D");
    GET_METHOD(B.c_Entity, m_ent_getY, "method_23318", "()D");
    GET_METHOD(B.c_Entity, m_ent_getZ, "method_23321", "()D");

    GET_FIELD(B.c_ClientPlayerEntity, f_cpe_input, "field_3913", "L" MC "class_744;");
    GET_FIELD(B.c_ClientPlayerEntity, f_cpe_nauseaIntensity, "field_44911", "F");
    GET_FIELD(B.c_ClientPlayerEntity, f_cpe_lastNauseaIntensity, "field_44912", "F");

    GET_FIELD(B.c_Input, f_in_playerInput, "field_54155", "L" MC "class_10185;");
    GET_FIELD(B.c_Input, f_in_movementVector, "field_55868", "L" MC "class_241;");
    GET_METHOD_OPT(B.c_Input, m_in_jump, "method_64054", "()V");

    GET_METHOD_OPT(B.c_PlayerInput, m_pi_init6, "<init>", "(ZZZZZZ)V");

    GET_METHOD(B.c_Vec2f, m_v2_init, "<init>", "(FF)V");

    GET_FIELD(B.c_Vec3d, f_v3_x, "field_1352", "D");
    GET_FIELD(B.c_Vec3d, f_v3_y, "field_1351", "D");
    GET_FIELD(B.c_Vec3d, f_v3_z, "field_1350", "D");

    GET_FIELD(B.c_Box, f_box_minX, "field_1323", "D");
    GET_FIELD(B.c_Box, f_box_minY, "field_1322", "D");
    GET_FIELD(B.c_Box, f_box_minZ, "field_1321", "D");
    GET_FIELD(B.c_Box, f_box_maxX, "field_1320", "D");
    GET_FIELD(B.c_Box, f_box_maxY, "field_1325", "D");
    GET_FIELD(B.c_Box, f_box_maxZ, "field_1324", "D");

    GET_METHOD(B.c_InteractionManager, m_im_attackEntity, "method_2918",
               "(L" MC "class_1657;L" MC "class_1297;)V");

    GET_FIELD(B.c_PlayerEntity, f_pe_inventory, "field_7514", "L" MC "class_1661;");
    GET_FIELD(B.c_PlayerInventory, f_inv_selectedSlot, "field_7545", "I");
    GET_FIELD(B.c_PlayerInventory, f_inv_main, "field_7547", "L" MC "class_2371;");
    GET_METHOD_OPT(B.c_PlayerInventory, m_inv_setSelectedSlot, "method_61496", "(I)V");
    GET_METHOD(B.c_DefaultedList, m_dl_get, "get", "(I)Ljava/lang/Object;");

    GET_METHOD(B.c_ItemStack, m_is_getItem, "method_7909", "()L" MC "class_1792;");
    GET_METHOD(B.c_ItemStack, m_is_isEmpty, "method_7960", "()Z");
    GET_METHOD(B.c_Item, m_i_getTranslationKey, "method_7876", "()Ljava/lang/String;");

    GET_FIELD(B.c_ClientWorld, f_world_players, "field_18226", "Ljava/util/List;");
    GET_METHOD(B.c_List, m_list_size, "size", "()I");
    GET_METHOD(B.c_List, m_list_get, "get", "(I)Ljava/lang/Object;");

    GET_METHOD_OPT(B.c_LivingEntity, m_liv_getHealth, "method_6032", "()F");

    bindings_ready = true;
    return true;
}

JNIEnv* env() {
    if (!g_vm) return nullptr;
    JNIEnv* e = nullptr;
    jint rc = g_vm->GetEnv((void**)&e, JNI_VERSION_1_8);
    if (rc == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread((void**)&e, nullptr) != JNI_OK)
            return nullptr;
        g_attached = true;
    } else if (rc != JNI_OK) {
        return nullptr;
    }
    return e;
}

bool attach(JavaVM* vm) {
    g_vm = vm;
    JNIEnv* e = env();
    if (!e) return false;
    return init(e);
}

void shutdown() {
    if (g_vm && g_attached)
        g_vm->DetachCurrentThread();
    g_attached = false;
    g_vm = nullptr;
    bindings_ready = false;
}

jobject mc_client(JNIEnv* e) {
    if (!bindings_ready) return nullptr;
    jobject r = e->CallStaticObjectMethod(B.c_MinecraftClient, B.m_getInstance);
    if (e->ExceptionCheck()) {
        e->ExceptionClear();
        return nullptr;
    }
    return r;
}

jobject player(JNIEnv* e) {
    jobject c = mc_client(e);
    if (!c) return nullptr;
    jobject r = e->GetObjectField(c, B.f_player);
    e->DeleteLocalRef(c);
    if (!r) e->ExceptionClear();
    return r;
}

jobject world(JNIEnv* e) {
    jobject c = mc_client(e);
    if (!c) return nullptr;
    jobject r = e->GetObjectField(c, B.f_world);
    e->DeleteLocalRef(c);
    if (!r) e->ExceptionClear();
    return r;
}

jobject game_renderer(JNIEnv* e) {
    jobject c = mc_client(e);
    if (!c) return nullptr;
    jobject r = e->GetObjectField(c, B.f_gameRenderer);
    e->DeleteLocalRef(c);
    if (!r) e->ExceptionClear();
    return r;
}

jobject interaction_manager(JNIEnv* e) {
    jobject c = mc_client(e);
    if (!c) return nullptr;
    jobject r = e->GetObjectField(c, B.f_interactionManager);
    e->DeleteLocalRef(c);
    if (!r) e->ExceptionClear();
    return r;
}

jobject camera(JNIEnv* e) {
    jobject gr = game_renderer(e);
    if (!gr) return nullptr;
    jobject r = e->GetObjectField(gr, B.f_gr_camera);
    e->DeleteLocalRef(gr);
    if (!r) e->ExceptionClear();
    return r;
}

jobject input(JNIEnv* e) {
    jobject p = player(e);
    if (!p) return nullptr;
    jobject r = e->GetObjectField(p, B.f_cpe_input);
    e->DeleteLocalRef(p);
    if (!r) e->ExceptionClear();
    return r;
}

bool screen_open(JNIEnv* e) {
    if (!bindings_ready) return false;
    if (!B.f_currentScreen) return false;
    jobject c = mc_client(e);
    if (!c) return false;
    jobject s = e->GetObjectField(c, B.f_currentScreen);
    e->DeleteLocalRef(c);
    if (!s) {
        e->ExceptionClear();
        return false;
    }
    e->DeleteLocalRef(s);
    return true;
}

bool in_game(JNIEnv* e) {
    jobject c = mc_client(e);
    if (!c) return false;
    jobject w = e->GetObjectField(c, B.f_world);
    e->DeleteLocalRef(c);
    if (!w) {
        e->ExceptionClear();
        return false;
    }
    e->DeleteLocalRef(w);
    return true;
}

}
