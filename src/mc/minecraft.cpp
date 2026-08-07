#include "minecraft.h"

#include "../math/math.h"
#include "../summer/config.h"
#include "../summer/jvm.h"
#include "../util/log.h"

#include <string>
#include <utility>

namespace summer {
namespace mc {

using JVM = summer::JVM;

namespace {

// ---- resolved JNI ids (cached once the game is booted) ----
struct Ids {
    bool tried = false;
    bool ok = false;

    jclass mcCls = nullptr;
    jobject mcInstance = nullptr;  // global ref
    jmethodID m_getInstance = nullptr;
    jfieldID f_instance = nullptr, f_theMinecraft = nullptr;
    jfieldID f_player = nullptr, f_level = nullptr, f_options = nullptr;
    jfieldID f_gameRenderer = nullptr, f_screen = nullptr, f_gameMode = nullptr;
    jfieldID f_mouseHandler = nullptr;

    jclass entityCls = nullptr, livingCls = nullptr, playerCls = nullptr;
    jclass localPlayerCls = nullptr, levelCls = nullptr;
    jclass aabbCls = nullptr, vec3Cls = nullptr;
    jclass optionsCls = nullptr, keyMappingCls = nullptr;
    jclass gameRendererCls = nullptr, cameraCls = nullptr;
    jclass gameModeCls = nullptr, mouseHandlerCls = nullptr;
    jclass inventoryCls = nullptr, inputCls = nullptr, foodDataCls = nullptr;
    jclass interactionHandCls = nullptr, teamCls = nullptr, componentCls = nullptr;
    jclass entityTypeCls = nullptr, optionInstanceCls = nullptr;

    jmethodID e_getX = nullptr, e_getY = nullptr, e_getZ = nullptr;
    jfieldID e_position = nullptr;
    jmethodID e_getBoundingBox = nullptr, e_isAlive = nullptr, e_isInvisible = nullptr;
    jmethodID e_getTeam = nullptr, e_getName = nullptr, e_getDisplayName = nullptr;
    jmethodID e_getType = nullptr;
    jfieldID e_yRot = nullptr, e_xRot = nullptr;
    jmethodID e_getYRot = nullptr, e_getXRot = nullptr;
    jmethodID e_setYRot = nullptr, e_setXRot = nullptr;
    jmethodID e_getEyeHeight = nullptr, e_getEyePosition = nullptr;
    jmethodID e_setFireTicks = nullptr;
    jfieldID e_fireTicks = nullptr;

    jmethodID l_getHealth = nullptr, l_getMaxHealth = nullptr;
    jmethodID l_getHurtTime = nullptr;
    jfieldID l_hurtTime = nullptr;
    jmethodID l_getAttackStrengthScale = nullptr, l_setSprinting = nullptr;
    jmethodID l_isSprinting = nullptr;
    jmethodID l_swingHand = nullptr, l_swingNoArg = nullptr;

    jmethodID p_getInventory = nullptr, p_getInput = nullptr, p_getFoodData = nullptr;
    jmethodID p_attack = nullptr;

    jfieldID lvl_entitiesById = nullptr;

    jfieldID a_minX = nullptr, a_minY = nullptr, a_minZ = nullptr;
    jfieldID a_maxX = nullptr, a_maxY = nullptr, a_maxZ = nullptr;
    jfieldID v_x = nullptr, v_y = nullptr, v_z = nullptr;

    jfieldID o_keyUp = nullptr, o_keyDown = nullptr, o_keyLeft = nullptr;
    jfieldID o_keyRight = nullptr, o_keyJump = nullptr, o_keySprint = nullptr;
    jfieldID o_keySneak = nullptr, o_keyAttack = nullptr, o_keyUse = nullptr;
    jfieldID o_gamma = nullptr, o_bobView = nullptr;

    jmethodID km_isDown = nullptr, km_setDown = nullptr;
    jmethodID oi_set = nullptr, oi_get = nullptr;

    jmethodID gr_getFov = nullptr, gr_getMainCamera = nullptr;
    jmethodID cam_getPosition = nullptr, cam_getYRot = nullptr, cam_getXRot = nullptr;

    jmethodID gm_attack = nullptr;

    jfieldID m_xpos = nullptr, m_ypos = nullptr, m_deltaWheel = nullptr;

    jfieldID i_selected = nullptr;
    jmethodID i_setSelected = nullptr;

    jfieldID in_forwardImpulse = nullptr, in_leftImpulse = nullptr;
    jfieldID in_up = nullptr, in_down = nullptr, in_left = nullptr, in_right = nullptr;
    jfieldID in_jumping = nullptr;

    jmethodID et_getDescriptionId = nullptr;
    jmethodID comp_getString = nullptr;
    jmethodID team_isAlliedTo = nullptr;
    jfieldID handMainHand = nullptr;
    jmethodID fd_getFoodLevel = nullptr;

    jclass itemStackCls = nullptr, itemCls = nullptr, nonNullListCls = nullptr;
    jclass listCls = nullptr;
    jfieldID i_items = nullptr;
    jmethodID is_getItem = nullptr;
    jmethodID item_getDescriptionId = nullptr;
    jmethodID list_get = nullptr;
};

static Ids ids;

static std::string mcSig, entitySig, livingSig, playerSig, levelSig, aabbSig;
static std::string vec3Sig, optionsSig, keyMappingSig, cameraSig, componentSig;
static std::string teamSig, entityTypeSig, inventorySig, inputSig;
static std::string interactionHandSig, foodDataSig, optionInstanceSig;

std::string Sig(jclass cls) { return JVM::ClassSig(cls); }

// ---- small helpers ----
jobject GetField(JNIEnv* env, jobject obj, jfieldID id) {
    if (!env || !obj || !id) return nullptr;
    return env->GetObjectField(obj, id);
}

float GetFloatField(JNIEnv* env, jobject obj, jfieldID id) {
    return (env && obj && id) ? env->GetFloatField(obj, id) : 0.f;
}

double GetDoubleField(JNIEnv* env, jobject obj, jfieldID id) {
    return (env && obj && id) ? env->GetDoubleField(obj, id) : 0.0;
}

int GetIntField(JNIEnv* env, jobject obj, jfieldID id) {
    return (env && obj && id) ? env->GetIntField(obj, id) : 0;
}

jobject GetPlayer(JNIEnv* env) {
    return GetField(env, ids.mcInstance, ids.f_player);
}

jobject GetOptions(JNIEnv* env) {
    return GetField(env, ids.mcInstance, ids.f_options);
}

std::string ComponentString(JNIEnv* env, jobject component) {
    if (!component) return "";
    jstring s = (jstring)env->CallObjectMethod(component, ids.comp_getString);
    if (env->ExceptionCheck() || !s) {
        env->ExceptionClear();
        return "";
    }
    const char* utf = env->GetStringUTFChars(s, nullptr);
    std::string out = utf ? utf : "";
    env->ReleaseStringUTFChars(s, utf);
    env->DeleteLocalRef(s);
    return out;
}

Vec3 EntityPosition(JNIEnv* env, jobject ent) {
    Vec3 p;
    if (ids.e_getX && ids.e_getY && ids.e_getZ) {
        p.x = env->CallDoubleMethod(ent, ids.e_getX);
        p.y = env->CallDoubleMethod(ent, ids.e_getY);
        p.z = env->CallDoubleMethod(ent, ids.e_getZ);
        env->ExceptionClear();
        return p;
    }
    if (ids.e_position) {
        jobject v = env->GetObjectField(ent, ids.e_position);
        if (v) {
            p.x = env->GetDoubleField(v, ids.v_x);
            p.y = env->GetDoubleField(v, ids.v_y);
            p.z = env->GetDoubleField(v, ids.v_z);
            env->DeleteLocalRef(v);
        }
    }
    return p;
}

AABB EntityBox(JNIEnv* env, jobject ent) {
    AABB b;
    if (!ids.e_getBoundingBox || !ids.aabbCls) return b;
    jobject aabb = env->CallObjectMethod(ent, ids.e_getBoundingBox);
    if (env->ExceptionCheck() || !aabb) {
        env->ExceptionClear();
        return b;
    }
    b.minX = env->GetDoubleField(aabb, ids.a_minX);
    b.minY = env->GetDoubleField(aabb, ids.a_minY);
    b.minZ = env->GetDoubleField(aabb, ids.a_minZ);
    b.maxX = env->GetDoubleField(aabb, ids.a_maxX);
    b.maxY = env->GetDoubleField(aabb, ids.a_maxY);
    b.maxZ = env->GetDoubleField(aabb, ids.a_maxZ);
    env->DeleteLocalRef(aabb);
    return b;
}

bool IsHostileName(const std::string& n) {
    static const char* hosts[] = {
        "zombie",  "skeleton", "spider",   "creeper",    "enderman",
        "witch",   "phantom",  "hoglin",   "zombified_piglin", "wither",
        "blaze",   "ghast",    "slime",    "magma_cube", "guardian",
        "shulker", "vindicator", "pillager", "evoker",    "ravager",
        "vex",     "drowned",  "husk",     "stray",      "cave_spider",
        "silverfish", "endermite", "warden", "breeze",    "bogged",
        "piglin",  "brute",    "pufferfish", "ghastling",
    };
    for (const char* h : hosts)
        if (n.find(h) != std::string::npos) return true;
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------

bool EnsureResolved() {
    if (ids.ok) return true;
    if (ids.tried) return false;
    ids.tried = true;

    JNIEnv* env = JVM::Env();
    if (!env) return false;

    // ---- classes ----
    ids.mcCls = JVM::FindFirstClass({"net/minecraft/client/Minecraft"});
    ids.entityCls = JVM::FindFirstClass({"net/minecraft/world/entity/Entity"});
    ids.livingCls = JVM::FindFirstClass({"net/minecraft/world/entity/LivingEntity"});
    ids.playerCls = JVM::FindFirstClass({"net/minecraft/world/entity/player/Player"});
    ids.localPlayerCls =
        JVM::FindFirstClass({"net/minecraft/client/player/LocalPlayer"});
    ids.levelCls = JVM::FindFirstClass({"net/minecraft/client/multiplayer/ClientLevel"});
    ids.aabbCls = JVM::FindFirstClass({"net/minecraft/world/phys/AABB"});
    ids.vec3Cls = JVM::FindFirstClass({"net/minecraft/world/phys/Vec3"});
    ids.optionsCls = JVM::FindFirstClass({"net/minecraft/client/Options"});
    ids.keyMappingCls = JVM::FindFirstClass({"net/minecraft/client/KeyMapping"});
    ids.gameRendererCls =
        JVM::FindFirstClass({"net/minecraft/client/renderer/GameRenderer"});
    ids.cameraCls = JVM::FindFirstClass({"net/minecraft/client/Camera"});
    ids.gameModeCls =
        JVM::FindFirstClass({"net/minecraft/client/multiplayer/MultiPlayerGameMode"});
    ids.mouseHandlerCls = JVM::FindFirstClass({"net/minecraft/client/MouseHandler"});
    ids.inventoryCls =
        JVM::FindFirstClass({"net/minecraft/world/entity/player/Inventory"});
    ids.inputCls = JVM::FindFirstClass({"net/minecraft/client/player/Input"});
    ids.foodDataCls = JVM::FindFirstClass({"net/minecraft/world/food/FoodData"});
    ids.interactionHandCls = JVM::FindFirstClass({"net/minecraft/world/InteractionHand"});
    ids.teamCls = JVM::FindFirstClass({"net/minecraft/world/scores/Team"});
    ids.componentCls = JVM::FindFirstClass({"net/minecraft/network/chat/Component"});
    ids.entityTypeCls = JVM::FindFirstClass({"net/minecraft/world/entity/EntityType"});
    ids.optionInstanceCls =
        JVM::FindFirstClass({"net/minecraft/client/OptionInstance"});

    if (!ids.mcCls || !ids.entityCls || !ids.playerCls) {
        LogWarn("[Summer] failed to resolve core classes (game not loaded yet?)");
        ids.tried = false;  // allow retry
        return false;
    }

    mcSig = Sig(ids.mcCls);
    entitySig = Sig(ids.entityCls);
    livingSig = Sig(ids.livingCls);
    playerSig = Sig(ids.playerCls);
    levelSig = Sig(ids.levelCls);
    aabbSig = Sig(ids.aabbCls);
    vec3Sig = Sig(ids.vec3Cls);
    optionsSig = Sig(ids.optionsCls);
    keyMappingSig = Sig(ids.keyMappingCls);
    cameraSig = Sig(ids.cameraCls);
    componentSig = Sig(ids.componentCls);
    teamSig = Sig(ids.teamCls);
    entityTypeSig = Sig(ids.entityTypeCls);
    inventorySig = Sig(ids.inventoryCls);
    inputSig = Sig(ids.inputCls);
    interactionHandSig = Sig(ids.interactionHandCls);
    foodDataSig = Sig(ids.foodDataCls);
    optionInstanceSig = Sig(ids.optionInstanceCls);

    // ---- Minecraft singleton ----
    ids.m_getInstance =
        JVM::FindMethod("Minecraft.getInstance", ids.mcCls, ("()" + mcSig).c_str(),
                        true, {"getInstance"});
    ids.f_instance = JVM::FindField("Minecraft.instance", ids.mcCls, mcSig.c_str(),
                                    {"instance"});
    ids.f_theMinecraft = JVM::FindField("Minecraft.theMinecraft", ids.mcCls,
                                        mcSig.c_str(), {"theMinecraft"});

    if (ids.m_getInstance)
        ids.mcInstance = env->NewGlobalRef(
            env->CallStaticObjectMethod(ids.mcCls, ids.m_getInstance));
    else if (ids.f_instance)
        ids.mcInstance =
            env->NewGlobalRef(env->GetStaticObjectField(ids.mcCls, ids.f_instance));
    else if (ids.f_theMinecraft)
        ids.mcInstance = env->NewGlobalRef(
            env->GetStaticObjectField(ids.mcCls, ids.f_theMinecraft));
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!ids.mcInstance) {
        LogWarn("[Summer] could not resolve Minecraft instance");
        ids.tried = false;
        return false;
    }

    // ---- Minecraft fields ----
    ids.f_player = JVM::FindField("Minecraft.player", ids.mcCls,
                                  (localPlayerSig = Sig(ids.localPlayerCls)).c_str(),
                                  {"player"});
    ids.f_level = JVM::FindField("Minecraft.level", ids.mcCls, levelSig.c_str(),
                                 {"level", "clientLevel"});
    ids.f_options = JVM::FindField("Minecraft.options", ids.mcCls, optionsSig.c_str(),
                                   {"options"});
    ids.f_gameRenderer = JVM::FindField("Minecraft.gameRenderer", ids.mcCls,
                                        Sig(ids.gameRendererCls).c_str(),
                                        {"gameRenderer"});
    ids.f_screen = JVM::FindField("Minecraft.screen", ids.mcCls,
                                  Sig(JVM::FindFirstClass(
                                          {"net/minecraft/client/gui/screens/Screen"}))
                                      .c_str(),
                                  {"screen"});
    ids.f_gameMode = JVM::FindField("Minecraft.gameMode", ids.mcCls,
                                    Sig(ids.gameModeCls).c_str(), {"gameMode"});
    ids.f_mouseHandler = JVM::FindField("Minecraft.mouseHandler", ids.mcCls,
                                        Sig(ids.mouseHandlerCls).c_str(),
                                        {"mouseHandler"});

    // ---- Entity ----
    ids.e_getX = JVM::FindMethod("Entity.getX", ids.entityCls, "()D", false, {"getX"});
    ids.e_getY = JVM::FindMethod("Entity.getY", ids.entityCls, "()D", false, {"getY"});
    ids.e_getZ = JVM::FindMethod("Entity.getZ", ids.entityCls, "()D", false, {"getZ"});
    ids.e_position =
        JVM::FindField("Entity.position", ids.entityCls, vec3Sig.c_str(), {"position"});
    ids.e_getBoundingBox = JVM::FindMethod("Entity.getBoundingBox", ids.entityCls,
                                           ("()" + aabbSig).c_str(), false,
                                           {"getBoundingBox"});
    ids.e_isAlive = JVM::FindMethod("Entity.isAlive", ids.entityCls, "()Z", false,
                                    {"isAlive"});
    ids.e_isInvisible = JVM::FindMethod("Entity.isInvisible", ids.entityCls, "()Z",
                                        false, {"isInvisible"});
    ids.e_getTeam = JVM::FindMethod("Entity.getTeam", ids.entityCls,
                                    ("()" + teamSig).c_str(), false, {"getTeam"});
    ids.e_getName = JVM::FindMethod("Entity.getName", ids.entityCls,
                                    ("()" + componentSig).c_str(), false, {"getName"});
    ids.e_getDisplayName = JVM::FindMethod("Entity.getDisplayName", ids.entityCls,
                                           ("()" + componentSig).c_str(), false,
                                           {"getDisplayName"});
    ids.e_getType = JVM::FindMethod("Entity.getType", ids.entityCls,
                                    ("()" + entityTypeSig).c_str(), false, {"getType"});
    ids.e_yRot = JVM::FindField("Entity.yRot", ids.entityCls, "F", {"yRot"});
    ids.e_xRot = JVM::FindField("Entity.xRot", ids.entityCls, "F", {"xRot"});
    ids.e_getYRot = JVM::FindMethod("Entity.getYRot", ids.entityCls, "()F", false,
                                    {"getYRot", "getYHeadRot"});
    ids.e_getXRot = JVM::FindMethod("Entity.getXRot", ids.entityCls, "()F", false,
                                    {"getXRot"});
    ids.e_setYRot = JVM::FindMethod("Entity.setYRot", ids.entityCls, "(F)V", false,
                                    {"setYRot"});
    ids.e_setXRot = JVM::FindMethod("Entity.setXRot", ids.entityCls, "(F)V", false,
                                    {"setXRot"});
    ids.e_getEyeHeight = JVM::FindMethod("Entity.getEyeHeight", ids.entityCls, "()F",
                                         false, {"getEyeHeight"});
    ids.e_getEyePosition = JVM::FindMethod("Entity.getEyePosition", ids.entityCls,
                                           ("(F)" + vec3Sig).c_str(), false,
                                           {"getEyePosition"});
    ids.e_setFireTicks = JVM::FindMethod("Entity.setRemainingFireTicks",
                                         ids.entityCls, "(I)V", false,
                                         {"setRemainingFireTicks", "setFireTicks"});
    ids.e_fireTicks = JVM::FindField("Entity.remainingFireTicks", ids.entityCls, "I",
                                     {"remainingFireTicks", "fireTicks"});

    // ---- LivingEntity ----
    ids.l_getHealth = JVM::FindMethod("LivingEntity.getHealth", ids.livingCls, "()F",
                                      false, {"getHealth"});
    ids.l_getMaxHealth = JVM::FindMethod("LivingEntity.getMaxHealth", ids.livingCls,
                                         "()F", false, {"getMaxHealth"});
    ids.l_getHurtTime = JVM::FindMethod("LivingEntity.getHurtTime", ids.livingCls,
                                        "()I", false, {"getHurtTime"});
    ids.l_hurtTime = JVM::FindField("LivingEntity.hurtTime", ids.livingCls, "I",
                                    {"hurtTime", "invulnerableTime"});
    ids.l_getAttackStrengthScale = JVM::FindMethod(
        "LivingEntity.getAttackStrengthScale", ids.livingCls, "(F)F", false,
        {"getAttackStrengthScale"});
    ids.l_setSprinting = JVM::FindMethod("LivingEntity.setSprinting", ids.livingCls,
                                         "(Z)V", false, {"setSprinting"});
    ids.l_isSprinting = JVM::FindMethod("LivingEntity.isSprinting", ids.livingCls,
                                        "()Z", false, {"isSprinting"});
    ids.l_swingHand = JVM::FindMethod("LivingEntity.swingHand", ids.livingCls,
                                      ("(L" + interactionHandSig + ";)V").c_str(),
                                      false, {"swing"});
    ids.l_swingNoArg =
        JVM::FindMethod("LivingEntity.swingNoArg", ids.livingCls, "()V", false, {"swing"});

    // ---- Player ----
    ids.p_getInventory = JVM::FindMethod("Player.getInventory", ids.playerCls,
                                         ("()" + inventorySig).c_str(), false,
                                         {"getInventory"});
    ids.p_getInput = JVM::FindMethod("Player.getInput", ids.playerCls,
                                     ("()" + inputSig).c_str(), false, {"getInput"});
    ids.p_getFoodData = JVM::FindMethod("Player.getFoodData", ids.playerCls,
                                        ("()" + foodDataSig).c_str(), false,
                                        {"getFoodData"});
    ids.p_attack = JVM::FindMethod("Player.attack", ids.playerCls,
                                   ("(L" + entitySig + ";)V").c_str(), false,
                                   {"attack"});

    // ---- ClientLevel ----
    ids.lvl_entitiesById = JVM::FindField(
        "Level.entitiesById", ids.levelCls,
        "Lit/unimi/dsi/fastutil/ints/Int2ObjectMap;", {"entitiesById"});

    // ---- AABB / Vec3 ----
    ids.a_minX = JVM::FindField("AABB.minX", ids.aabbCls, "D", {"minX"});
    ids.a_minY = JVM::FindField("AABB.minY", ids.aabbCls, "D", {"minY"});
    ids.a_minZ = JVM::FindField("AABB.minZ", ids.aabbCls, "D", {"minZ"});
    ids.a_maxX = JVM::FindField("AABB.maxX", ids.aabbCls, "D", {"maxX"});
    ids.a_maxY = JVM::FindField("AABB.maxY", ids.aabbCls, "D", {"maxY"});
    ids.a_maxZ = JVM::FindField("AABB.maxZ", ids.aabbCls, "D", {"maxZ"});
    ids.v_x = JVM::FindField("Vec3.x", ids.vec3Cls, "D", {"x"});
    ids.v_y = JVM::FindField("Vec3.y", ids.vec3Cls, "D", {"y"});
    ids.v_z = JVM::FindField("Vec3.z", ids.vec3Cls, "D", {"z"});

    // ---- Options ----
    ids.o_keyUp = JVM::FindField("Options.keyUp", ids.optionsCls,
                                 keyMappingSig.c_str(), {"keyUp"});
    ids.o_keyDown = JVM::FindField("Options.keyDown", ids.optionsCls,
                                   keyMappingSig.c_str(), {"keyDown"});
    ids.o_keyLeft = JVM::FindField("Options.keyLeft", ids.optionsCls,
                                   keyMappingSig.c_str(), {"keyLeft"});
    ids.o_keyRight = JVM::FindField("Options.keyRight", ids.optionsCls,
                                    keyMappingSig.c_str(), {"keyRight"});
    ids.o_keyJump = JVM::FindField("Options.keyJump", ids.optionsCls,
                                   keyMappingSig.c_str(), {"keyJump"});
    ids.o_keySprint = JVM::FindField("Options.keySprint", ids.optionsCls,
                                     keyMappingSig.c_str(), {"keySprint"});
    ids.o_keySneak = JVM::FindField("Options.keySneak", ids.optionsCls,
                                    keyMappingSig.c_str(), {"keySneak"});
    ids.o_keyAttack = JVM::FindField("Options.keyAttack", ids.optionsCls,
                                     keyMappingSig.c_str(), {"keyAttack"});
    ids.o_keyUse = JVM::FindField("Options.keyUse", ids.optionsCls,
                                  keyMappingSig.c_str(), {"keyUseItem", "keyUse"});
    ids.o_gamma = JVM::FindField("Options.gamma", ids.optionsCls,
                                 optionInstanceSig.c_str(), {"gamma"});
    ids.o_bobView = JVM::FindField("Options.bobView", ids.optionsCls,
                                   optionInstanceSig.c_str(), {"bobView"});

    // ---- KeyMapping ----
    ids.km_isDown =
        JVM::FindMethod("KeyMapping.isDown", ids.keyMappingCls, "()Z", false, {"isDown"});
    ids.km_setDown = JVM::FindMethod("KeyMapping.setDown", ids.keyMappingCls, "(Z)V",
                                     false, {"setDown"});

    // ---- OptionInstance ----
    ids.oi_set = JVM::FindMethod("OptionInstance.set", ids.optionInstanceCls,
                                 "(Ljava/lang/Object;)Z", false, {"set"});
    ids.oi_get = JVM::FindMethod("OptionInstance.get", ids.optionInstanceCls,
                                 "()Ljava/lang/Object;", false, {"get"});

    // ---- GameRenderer / Camera ----
    ids.gr_getMainCamera = JVM::FindMethod("GameRenderer.getMainCamera",
                                           ids.gameRendererCls,
                                           ("()" + cameraSig).c_str(), false,
                                           {"getMainCamera", "getCamera"});
    ids.gr_getFov = JVM::FindMethod("GameRenderer.getFov", ids.gameRendererCls,
                                    ("(L" + cameraSig + ";FZ)D").c_str(), false,
                                    {"getFov"});
    if (!ids.gr_getFov)
        ids.gr_getFov = JVM::FindMethod("GameRenderer.getFovF", ids.gameRendererCls,
                                        ("(L" + cameraSig + ";FZ)F").c_str(), false,
                                        {"getFov"});
    ids.cam_getPosition = JVM::FindMethod("Camera.getPosition", ids.cameraCls,
                                          ("()" + vec3Sig).c_str(), false,
                                          {"getPosition"});
    ids.cam_getYRot =
        JVM::FindMethod("Camera.getYRot", ids.cameraCls, "()F", false, {"getYRot"});
    ids.cam_getXRot =
        JVM::FindMethod("Camera.getXRot", ids.cameraCls, "()F", false, {"getXRot"});

    // ---- MultiPlayerGameMode ----
    ids.gm_attack = JVM::FindMethod(
        "GameMode.attack", ids.gameModeCls,
        ("(L" + playerSig + ";L" + entitySig + ";)V").c_str(), false, {"attack"});

    // ---- MouseHandler ----
    ids.m_xpos = JVM::FindField("MouseHandler.xpos", ids.mouseHandlerCls, "D",
                                {"xpos", "lastMouseX"});
    ids.m_ypos = JVM::FindField("MouseHandler.ypos", ids.mouseHandlerCls, "D",
                                {"ypos", "lastMouseY"});
    ids.m_deltaWheel = JVM::FindField("MouseHandler.deltaWheel", ids.mouseHandlerCls,
                                      "D", {"deltaWheel"});

    // ---- Inventory ----
    ids.i_selected =
        JVM::FindField("Inventory.selected", ids.inventoryCls, "I", {"selected"});
    ids.i_setSelected = JVM::FindMethod("Inventory.setSelectedSlot", ids.inventoryCls,
                                        "(I)V", false, {"setSelectedSlot"});

    // ---- Item stack helpers ----
    ids.itemStackCls = JVM::FindFirstClass({"net/minecraft/world/item/ItemStack"});
    ids.itemCls = JVM::FindFirstClass({"net/minecraft/world/item/Item"});
    ids.nonNullListCls = JVM::FindFirstClass({"net/minecraft/core/NonNullList"});
    ids.listCls = JVM::FindClass("java/util/List");
    ids.i_items = JVM::FindField("Inventory.items", ids.inventoryCls,
                                 Sig(ids.nonNullListCls).c_str(), {"items"});
    ids.is_getItem = JVM::FindMethod("ItemStack.getItem", ids.itemStackCls,
                                     ("()" + Sig(ids.itemCls)).c_str(), false,
                                     {"getItem"});
    ids.item_getDescriptionId = JVM::FindMethod(
        "Item.getDescriptionId", ids.itemCls, "()Ljava/lang/String;", false,
        {"getDescriptionId"});
    ids.list_get = JVM::FindMethod("List.get", ids.listCls, "(I)Ljava/lang/Object;",
                                   false, {"get"});

    // ---- Input ----
    ids.in_forwardImpulse = JVM::FindField("Input.forwardImpulse", ids.inputCls, "F",
                                           {"forwardImpulse"});
    ids.in_leftImpulse = JVM::FindField("Input.leftImpulse", ids.inputCls, "F",
                                        {"leftImpulse"});
    ids.in_up = JVM::FindField("Input.up", ids.inputCls, "Z", {"up"});
    ids.in_down = JVM::FindField("Input.down", ids.inputCls, "Z", {"down"});
    ids.in_left = JVM::FindField("Input.left", ids.inputCls, "Z", {"left"});
    ids.in_right = JVM::FindField("Input.right", ids.inputCls, "Z", {"right"});
    ids.in_jumping = JVM::FindField("Input.jumping", ids.inputCls, "Z", {"jumping"});

    // ---- EntityType / Component / Team / InteractionHand / FoodData ----
    ids.et_getDescriptionId = JVM::FindMethod("EntityType.getDescriptionId",
                                              ids.entityTypeCls,
                                              "()Ljava/lang/String;", false,
                                              {"getDescriptionId"});
    ids.comp_getString = JVM::FindMethod("Component.getString", ids.componentCls,
                                         "()Ljava/lang/String;", false, {"getString"});
    ids.team_isAlliedTo = JVM::FindMethod(
        "Team.isAlliedTo", ids.teamCls, ("(L" + teamSig + ";)Z").c_str(), false,
        {"isAlliedTo"});
    ids.handMainHand = JVM::FindField("InteractionHand.MAIN_HAND",
                                      ids.interactionHandCls,
                                      ("L" + interactionHandSig + ";").c_str(),
                                      {"MAIN_HAND"});
    ids.fd_getFoodLevel = JVM::FindMethod("FoodData.getFoodLevel", ids.foodDataCls,
                                          "()I", false, {"getFoodLevel"});

    ids.ok = ids.f_player && ids.f_level && ids.e_getX && ids.e_getBoundingBox;
    if (!ids.ok) {
        LogWarn("[Summer] some mappings failed to resolve - run class dump and "
                "fill map.* overrides");
    } else {
        Log("[Summer] Minecraft layer resolved (ok)");
    }
    return ids.ok;
}

bool InGame() { return ids.ok && ids.mcInstance != nullptr; }

// ---------------------------------------------------------------------------

WorldSnapshot Capture() {
    WorldSnapshot s;
    JNIEnv* env = JVM::Env();
    if (!env) return s;
    if (!EnsureResolved()) return s;

    jobject local = GetPlayer(env);
    if (!local) return s;

    s.valid = true;
    s.hasScreen = ids.f_screen ? GetField(env, ids.mcInstance, ids.f_screen) != nullptr
                               : false;

    // local player data
    Vec3 pos = EntityPosition(env, local);
    float yaw = ids.e_getYRot ? env->CallFloatMethod(local, ids.e_getYRot)
                              : GetFloatField(env, local, ids.e_yRot);
    float pitch = ids.e_getXRot ? env->CallFloatMethod(local, ids.e_getXRot)
                                : GetFloatField(env, local, ids.e_xRot);
    float eyeH = ids.e_getEyeHeight ? env->CallFloatMethod(local, ids.e_getEyeHeight)
                                    : 1.62f;
    s.localPos = pos;
    s.eyePos = Vec3(pos.x, pos.y + eyeH, pos.z);
    s.localYaw = yaw;
    s.localPitch = pitch;

    // fov from game renderer (fallback 70)
    float fov = 70.f;
    if (ids.gr_getFov && ids.gr_getMainCamera && ids.f_gameRenderer) {
        jobject gr = GetField(env, ids.mcInstance, ids.f_gameRenderer);
        jobject cam = gr ? env->CallObjectMethod(gr, ids.gr_getMainCamera) : nullptr;
        if (!env->ExceptionCheck() && cam) {
            jdouble d = env->CallDoubleMethod(gr, ids.gr_getFov, cam, 1.0f, JNI_TRUE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else fov = (float)d;
        } else {
            env->ExceptionClear();
        }
        if (cam) env->DeleteLocalRef(cam);
        if (gr) env->DeleteLocalRef(gr);
    }
    s.fov = fov;

    // local player snapshot entry
    EntitySnap self;
    self.ref = local;
    self.isLocal = true;
    self.isPlayer = true;
    self.isAlive = ids.e_isAlive ? env->CallBooleanMethod(local, ids.e_isAlive) : true;
    if (ids.l_getHealth)
        self.health = env->CallFloatMethod(local, ids.l_getHealth);
    if (ids.l_getMaxHealth)
        self.maxHealth = env->CallFloatMethod(local, ids.l_getMaxHealth);
    self.yRot = yaw;
    self.xRot = pitch;
    self.pos = pos;
    self.box = EntityBox(env, local);
    if (ids.l_getHurtTime) self.hurtTime = env->CallIntMethod(local, ids.l_getHurtTime);
    if (ids.l_getAttackStrengthScale)
        self.attackCooldownPct = (int)(env->CallFloatMethod(local,
                                    ids.l_getAttackStrengthScale, 0.f) * 100.f);
    self.name = "You";
    s.entities.push_back(self);
    s.localIndex = 0;

    // local team for allied checks
    jobject localTeam = ids.e_getTeam ? env->CallObjectMethod(local, ids.e_getTeam)
                                      : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    // world entities
    jobject level = GetField(env, ids.mcInstance, ids.f_level);
    if (level && ids.lvl_entitiesById) {
        jobject map = env->GetObjectField(level, ids.lvl_entitiesById);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (map) {
            jclass mapCls = env->GetObjectClass(map);
            jmethodID values =
                JVM::FindMethod("Int2ObjectMap.values", mapCls, "()Ljava/util/Collection;",
                                false, {"values"});
            if (values) {
                jobject col = env->CallObjectMethod(map, values);
                if (col) {
                    jclass colCls = env->GetObjectClass(col);
                    jmethodID toArray = JVM::FindMethod("Collection.toArray", colCls,
                                                        "()[Ljava/lang/Object;", false,
                                                        {"toArray"});
                    if (toArray) {
                        jobjectArray arr =
                            (jobjectArray)env->CallObjectMethod(col, toArray);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        jsize n = arr ? env->GetArrayLength(arr) : 0;
                        Vec3 viewDir = DirectionFromRot(yaw, pitch);
                        for (jsize i = 0; i < n; ++i) {
                            jobject ent = env->GetObjectArrayElement(arr, i);
                            if (!ent) continue;
                            bool isLocal = env->IsSameObject(ent, local) == JNI_TRUE;
                            if (isLocal) {
                                env->DeleteLocalRef(ent);
                                continue;
                            }
                            EntitySnap e;
                            e.ref = ent;
                            e.isLocal = false;
                            e.isPlayer =
                                env->IsInstanceOf(ent, ids.playerCls) == JNI_TRUE;
                            e.isAlive =
                                ids.e_isAlive
                                    ? env->CallBooleanMethod(ent, ids.e_isAlive)
                                    : true;
                            e.isInvisible =
                                ids.e_isInvisible
                                    ? env->CallBooleanMethod(ent, ids.e_isInvisible)
                                    : false;
                            e.isMob = !e.isPlayer;
                            e.box = EntityBox(env, ent);
                            e.pos = e.box.Center();
                            if (ids.e_getYRot)
                                e.yRot = env->CallFloatMethod(ent, ids.e_getYRot);
                            if (ids.e_getXRot)
                                e.xRot = env->CallFloatMethod(ent, ids.e_getXRot);
                            bool isLiving =
                                env->IsInstanceOf(ent, ids.livingCls) == JNI_TRUE;
                            if (isLiving) {
                                if (ids.l_getHealth)
                                    e.health = env->CallFloatMethod(ent, ids.l_getHealth);
                                if (ids.l_getMaxHealth)
                                    e.maxHealth =
                                        env->CallFloatMethod(ent, ids.l_getMaxHealth);
                                if (ids.l_getHurtTime)
                                    e.hurtTime = env->CallIntMethod(ent, ids.l_getHurtTime);
                            }

                            jobject team = ids.e_getTeam
                                               ? env->CallObjectMethod(ent, ids.e_getTeam)
                                               : nullptr;
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (team && localTeam && ids.team_isAlliedTo)
                                e.allied = env->CallBooleanMethod(
                                               team, ids.team_isAlliedTo, localTeam) ==
                                           JNI_TRUE;
                            if (team) env->DeleteLocalRef(team);

                            jobject nameC = ids.e_getDisplayName
                                                ? env->CallObjectMethod(
                                                      ent, ids.e_getDisplayName)
                                                : nullptr;
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (!nameC && ids.e_getName)
                                nameC = env->CallObjectMethod(ent, ids.e_getName);
                            e.name = ComponentString(env, nameC);
                            if (nameC) env->DeleteLocalRef(nameC);

                            if (e.isMob) {
                                e.hostile = IsHostileName(e.name);
                            } else {
                                e.hostile = !e.allied;
                            }

                            // distance / fov angle
                            Vec3 toCenter = e.pos - s.eyePos;
                            e.dist = toCenter.Length();
                            if (e.dist > 256.0) {
                                env->DeleteLocalRef(ent);
                                continue;
                            }
                            double dot = toCenter.Dot(viewDir);
                            if (e.dist > 1e-9) {
                                double c = dot / e.dist;
                                c = c < -1.0 ? -1.0 : (c > 1.0 ? 1.0 : c);
                                e.fovAngle = std::acos(c) * 180.0 / kPi;
                            }
                            s.entities.push_back(std::move(e));
                        }
                        if (arr) env->DeleteLocalRef(arr);
                    }
                    env->DeleteLocalRef(col);
                }
            }
            env->DeleteLocalRef(map);
        }
        env->DeleteLocalRef(level);
    }
    if (localTeam) env->DeleteLocalRef(localTeam);

    // viewport will be filled by overlay
    return s;
}

// ---------------------------------------------------------------------------

void SetRotation(float yaw, float pitch) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    if (ids.e_setYRot)
        env->CallVoidMethod(local, ids.e_setYRot, (jfloat)yaw);
    else if (ids.e_yRot)
        env->SetFloatField(local, ids.e_yRot, (jfloat)yaw);
    if (ids.e_setXRot)
        env->CallVoidMethod(local, ids.e_setXRot, (jfloat)pitch);
    else if (ids.e_xRot)
        env->SetFloatField(local, ids.e_xRot, (jfloat)pitch);
    env->DeleteLocalRef(local);
}

void Attack(jobject target) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    if (ids.gm_attack && ids.f_gameMode) {
        jobject gm = GetField(env, ids.mcInstance, ids.f_gameMode);
        if (gm) {
            env->CallVoidMethod(gm, ids.gm_attack, local, target);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(gm);
        }
    } else if (ids.p_attack) {
        env->CallVoidMethod(local, ids.p_attack, target);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    env->DeleteLocalRef(local);
}

void Swing() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    if (ids.l_swingHand && ids.handMainHand) {
        jobject hand = env->GetStaticObjectField(ids.interactionHandCls, ids.handMainHand);
        if (hand) {
            env->CallVoidMethod(local, ids.l_swingHand, hand);
            env->DeleteLocalRef(hand);
        }
    } else if (ids.l_swingNoArg) {
        env->CallVoidMethod(local, ids.l_swingNoArg);
    }
    env->DeleteLocalRef(local);
}

bool SetSelectedSlot(int slot) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return false;
    jobject local = GetPlayer(env);
    if (!local) return false;
    bool out = false;
    jobject inv = ids.p_getInventory
                      ? env->CallObjectMethod(local, ids.p_getInventory)
                      : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (inv) {
        if (ids.i_setSelected) {
            env->CallVoidMethod(inv, ids.i_setSelected, (jint)slot);
            out = true;
        } else if (ids.i_selected) {
            env->SetIntField(inv, ids.i_selected, (jint)slot);
            out = true;
        }
        env->DeleteLocalRef(inv);
    }
    env->DeleteLocalRef(local);
    return out;
}

int GetSelectedSlot() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return -1;
    jobject local = GetPlayer(env);
    if (!local) return -1;
    int out = -1;
    jobject inv = ids.p_getInventory
                      ? env->CallObjectMethod(local, ids.p_getInventory)
                      : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (inv) {
        out = GetIntField(env, inv, ids.i_selected);
        env->DeleteLocalRef(inv);
    }
    env->DeleteLocalRef(local);
    return out;
}

void SetSprinting(bool v) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.l_setSprinting) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    env->CallVoidMethod(local, ids.l_setSprinting, (jboolean)v);
    env->DeleteLocalRef(local);
}

bool IsSprinting() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.l_isSprinting) return false;
    jobject local = GetPlayer(env);
    if (!local) return false;
    bool out = env->CallBooleanMethod(local, ids.l_isSprinting) == JNI_TRUE;
    env->DeleteLocalRef(local);
    return out;
}

void SetKeySprint(bool down) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.o_keySprint || !ids.km_setDown) return;
    jobject options = GetOptions(env);
    if (!options) return;
    jobject km = env->GetObjectField(options, ids.o_keySprint);
    if (km) {
        env->CallVoidMethod(km, ids.km_setDown, (jboolean)down);
        env->DeleteLocalRef(km);
    }
    env->DeleteLocalRef(options);
}

static bool IsKeyDown(jfieldID kmField) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !kmField || !ids.km_isDown) return false;
    jobject options = GetOptions(env);
    if (!options) return false;
    jobject km = env->GetObjectField(options, kmField);
    bool out = false;
    if (km) {
        out = env->CallBooleanMethod(km, ids.km_isDown) == JNI_TRUE;
        env->DeleteLocalRef(km);
    }
    env->DeleteLocalRef(options);
    return out;
}

bool IsKeyAttackDown() { return IsKeyDown(ids.o_keyAttack); }
bool IsKeyUseDown() { return IsKeyDown(ids.o_keyUse); }

bool IsKeyKindDown(KeyKind k) {
    jfieldID f = nullptr;
    switch (k) {
        case KeyKind::Forward: f = ids.o_keyUp; break;
        case KeyKind::Back: f = ids.o_keyDown; break;
        case KeyKind::Left: f = ids.o_keyLeft; break;
        case KeyKind::Right: f = ids.o_keyRight; break;
        case KeyKind::Jump: f = ids.o_keyJump; break;
        case KeyKind::Sprint: f = ids.o_keySprint; break;
        case KeyKind::Sneak: f = ids.o_keySneak; break;
        case KeyKind::Attack: f = ids.o_keyAttack; break;
        case KeyKind::Use: f = ids.o_keyUse; break;
        default: return false;
    }
    return IsKeyDown(f);
}

void SetHurtTime(int ticks) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    if (ids.l_hurtTime)
        env->SetIntField(local, ids.l_hurtTime, (jint)ticks);
    env->DeleteLocalRef(local);
}

void SetFireTicks(int ticks) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    if (ids.e_setFireTicks)
        env->CallVoidMethod(local, ids.e_setFireTicks, (jint)ticks);
    else if (ids.e_fireTicks)
        env->SetIntField(local, ids.e_fireTicks, (jint)ticks);
    env->DeleteLocalRef(local);
}

double GetFoodLevel() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.p_getFoodData || !ids.fd_getFoodLevel) return 20.0;
    jobject local = GetPlayer(env);
    if (!local) return 20.0;
    jobject fd = env->CallObjectMethod(local, ids.p_getFoodData);
    double out = 20.0;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (fd) {
        out = env->CallIntMethod(fd, ids.fd_getFoodLevel);
        env->DeleteLocalRef(fd);
    }
    env->DeleteLocalRef(local);
    return out;
}

static jobject BoxedDouble(JNIEnv* env, double v) {
    jclass dbl = env->FindClass("java/lang/Double");
    jmethodID valueOf = env->GetStaticMethodID(dbl, "valueOf", "(D)Ljava/lang/Double;");
    return env->CallStaticObjectMethod(dbl, valueOf, (jdouble)v);
}

static jobject BoxedBool(JNIEnv* env, bool v) {
    jclass bl = env->FindClass("java/lang/Boolean");
    jmethodID valueOf = env->GetStaticMethodID(bl, "valueOf", "(Z)Ljava/lang/Boolean;");
    return env->CallStaticObjectMethod(bl, valueOf, (jboolean)v);
}

void SetOptionInstance(jfieldID optionField, jobject value) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !optionField || !ids.oi_set) return;
    jobject options = GetOptions(env);
    if (!options) return;
    jobject opt = env->GetObjectField(options, optionField);
    if (opt) {
        env->CallBooleanMethod(opt, ids.oi_set, value);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(opt);
    }
    env->DeleteLocalRef(options);
}

void SetGamma(double value) {
    JNIEnv* env = JVM::Env();
    if (!env) return;
    jobject boxed = BoxedDouble(env, value);
    SetOptionInstance(ids.o_gamma, boxed);
    env->DeleteLocalRef(boxed);
}

void SetBobView(bool enabled) {
    JNIEnv* env = JVM::Env();
    if (!env) return;
    jobject boxed = BoxedBool(env, enabled);
    SetOptionInstance(ids.o_bobView, boxed);
    env->DeleteLocalRef(boxed);
}

int ReadWheelDelta() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.f_mouseHandler || !ids.m_deltaWheel) return 0;
    jobject mh = GetField(env, ids.mcInstance, ids.f_mouseHandler);
    if (!mh) return 0;
    double d = env->GetDoubleField(mh, ids.m_deltaWheel);
    env->SetDoubleField(mh, ids.m_deltaWheel, 0.0);
    env->DeleteLocalRef(mh);
    return (int)d;
}

void ClearScreenInput() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok) return;
    jobject options = GetOptions(env);
    if (options) {
        for (jfieldID f : {ids.o_keyAttack, ids.o_keyUse, ids.o_keyJump,
                           ids.o_keySprint, ids.o_keySneak, ids.o_keyUp, ids.o_keyDown,
                           ids.o_keyLeft, ids.o_keyRight}) {
            if (!f || !ids.km_setDown) continue;
            jobject km = env->GetObjectField(options, f);
            if (km) {
                env->CallVoidMethod(km, ids.km_setDown, JNI_FALSE);
                env->DeleteLocalRef(km);
            }
        }
        env->DeleteLocalRef(options);
    }
    jobject local = GetPlayer(env);
    if (local && ids.p_getInput && ids.in_forwardImpulse) {
        jobject input = env->CallObjectMethod(local, ids.p_getInput);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (input) {
            env->SetFloatField(input, ids.in_forwardImpulse, 0.f);
            env->SetFloatField(input, ids.in_leftImpulse, 0.f);
            env->DeleteLocalRef(input);
        }
        env->DeleteLocalRef(local);
    }
}

float GetForwardImpulse() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.p_getInput || !ids.in_forwardImpulse) return 0.f;
    jobject local = GetPlayer(env);
    if (!local) return 0.f;
    float out = 0.f;
    jobject input = env->CallObjectMethod(local, ids.p_getInput);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (input) {
        out = env->GetFloatField(input, ids.in_forwardImpulse);
        env->DeleteLocalRef(input);
    }
    env->DeleteLocalRef(local);
    return out;
}

float GetLeftImpulse() {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.p_getInput || !ids.in_leftImpulse) return 0.f;
    jobject local = GetPlayer(env);
    if (!local) return 0.f;
    float out = 0.f;
    jobject input = env->CallObjectMethod(local, ids.p_getInput);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (input) {
        out = env->GetFloatField(input, ids.in_leftImpulse);
        env->DeleteLocalRef(input);
    }
    env->DeleteLocalRef(local);
    return out;
}

bool IsKeySneakDown() { return IsKeyDown(ids.o_keySneak); }

void SetInputMovement(float forward, float left, bool up, bool down, bool leftKey,
                      bool rightKey, bool jump) {
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || !ids.p_getInput) return;
    jobject local = GetPlayer(env);
    if (!local) return;
    jobject input = env->CallObjectMethod(local, ids.p_getInput);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (input) {
        if (ids.in_forwardImpulse)
            env->SetFloatField(input, ids.in_forwardImpulse, (jfloat)forward);
        if (ids.in_leftImpulse)
            env->SetFloatField(input, ids.in_leftImpulse, (jfloat)left);
        if (ids.in_up) env->SetBooleanField(input, ids.in_up, (jboolean)up);
        if (ids.in_down) env->SetBooleanField(input, ids.in_down, (jboolean)down);
        if (ids.in_left) env->SetBooleanField(input, ids.in_left, (jboolean)leftKey);
        if (ids.in_right) env->SetBooleanField(input, ids.in_right, (jboolean)rightKey);
        if (ids.in_jumping) env->SetBooleanField(input, ids.in_jumping, (jboolean)jump);
        env->DeleteLocalRef(input);
    }
    env->DeleteLocalRef(local);
}

bool GetHotbarItemName(int slot, std::string& out) {
    out.clear();
    JNIEnv* env = JVM::Env();
    if (!env || !ids.ok || slot < 0 || slot > 8) return false;
    jobject local = GetPlayer(env);
    if (!local) return false;
    bool ok = false;
    jobject inv = ids.p_getInventory ? env->CallObjectMethod(local, ids.p_getInventory)
                                     : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (inv) {
        jobject items =
            ids.i_items ? env->GetObjectField(inv, ids.i_items) : nullptr;
        if (items) {
            jobject stack = ids.list_get ? env->CallObjectMethod(items, ids.list_get,
                                                                 (jint)slot)
                                         : nullptr;
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            } else if (stack) {
                jobject item = ids.is_getItem ? env->CallObjectMethod(stack, ids.is_getItem)
                                              : nullptr;
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (item) {
                    jstring s = ids.item_getDescriptionId
                                    ? (jstring)env->CallObjectMethod(
                                          item, ids.item_getDescriptionId)
                                    : nullptr;
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (s) {
                        const char* utf = env->GetStringUTFChars(s, nullptr);
                        if (utf) {
                            out = utf;
                            env->ReleaseStringUTFChars(s, utf);
                            ok = true;
                        }
                        env->DeleteLocalRef(s);
                    }
                    env->DeleteLocalRef(item);
                }
                env->DeleteLocalRef(stack);
            }
            env->DeleteLocalRef(items);
        }
        env->DeleteLocalRef(inv);
    }
    env->DeleteLocalRef(local);
    return ok;
}

}  // namespace mc
}  // namespace summer
