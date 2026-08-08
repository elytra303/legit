#pragma once

#include <jni.h>

#include "types.h"

namespace summer {
namespace mc {

// Resolve + cache all needed classes/fields/methods. Safe to call every frame;
// returns false until the game has fully booted.
bool EnsureResolved();

// True when the Minecraft singleton is available.
bool InGame();

// Capture a world snapshot for the current frame. Caller must hold a
// JNIScope for the returned entity refs to stay valid.
WorldSnapshot Capture();

// --- actions (write back into the game) ---
void SetRotation(float yaw, float pitch);
void Attack(jobject targetEntity);
void Swing();
bool SetSelectedSlot(int slot);
int GetSelectedSlot();
void SetSprinting(bool v);
bool IsSprinting();
void SetKeySprint(bool down);
void SetKeyUse(bool down);

enum class KeyKind {
    Forward = 0,
    Back,
    Left,
    Right,
    Jump,
    Sprint,
    Sneak,
    Attack,
    Use,
    Count
};
bool IsKeyKindDown(KeyKind k);
void SetHurtTime(int ticks);
void SetFireTicks(int ticks);
double GetFoodLevel();
void SetGamma(double value);
void SetBobView(bool enabled);
int ReadWheelDelta();
void ClearScreenInput();  // neutralize player input while our GUI is open

void SetInputMovement(float forward, float left, bool up, bool down,
                      bool leftKey, bool rightKey, bool jump);
float GetForwardImpulse();
float GetLeftImpulse();
bool IsKeySneakDown();
bool IsKeyAttackDown();
bool IsKeyUseDown();
bool GetHotbarItemName(int slot, std::string& out);
bool GetInventoryItemName(int slot, std::string& out);
bool GetArmorItemName(int armorIndex, std::string& out);
bool InventoryQuickMove(int containerId, int slotId);

}  // namespace mc
}  // namespace summer
