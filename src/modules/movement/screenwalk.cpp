#include "screenwalk.h"

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../client.h"

namespace summer {

ScreenWalk::ScreenWalk()
    : Module("ScreenWalk",
             "Keep moving while a GUI screen is open (works on servers)",
             Category::Movement) {}

void ScreenWalk::Save(Config& c) {
    c.SetBool("screenwalk.enabled", Enabled());
    c.SetInt("screenwalk.key", Key());
    c.SetBool("screenwalk.forward", forward_);
    c.SetBool("screenwalk.strafe", strafe_);
    c.SetBool("screenwalk.jump", jump_);
}

void ScreenWalk::Load(const Config& c) {
    SetEnabled(c.GetBool("screenwalk.enabled", false));
    SetKey(c.GetInt("screenwalk.key", 0));
    forward_ = c.GetBool("screenwalk.forward", true);
    strafe_ = c.GetBool("screenwalk.strafe", true);
    jump_ = c.GetBool("screenwalk.jump", false);
}

void ScreenWalk::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid || !snap.hasScreen) return;

    bool up = mc::IsKeyKindDown(mc::KeyKind::Up);
    bool down = mc::IsKeyKindDown(mc::KeyKind::Down);
    bool left = mc::IsKeyKindDown(mc::KeyKind::Left);
    bool right = mc::IsKeyKindDown(mc::KeyKind::Right);
    bool jump = jump_ && mc::IsKeyKindDown(mc::KeyKind::Jump);

    float fwd = 0.f, str = 0.f;
    if (forward_) {
        if (up) fwd = 1.f;
        if (down) fwd = -1.f;
    }
    if (strafe_) {
        if (left) str = 1.f;
        if (right) str = -1.f;
    }
    mc::SetInputMovement(fwd, str, up, down, left, right, jump);
}

void ScreenWalk::DrawSettings() {
    gui::Checkbox("Forward / Back", &forward_);
    gui::Checkbox("Strafe", &strafe_);
    gui::Checkbox("Jump", &jump_);
    gui::Help("Re-applies the keys you physically hold while a screen is open");
}

}  // namespace summer
