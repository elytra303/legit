#include "autosprint.h"

#include <cmath>

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

AutoSprint::AutoSprint()
    : Module("AutoSprint", "Sprints automatically while walking forward",
             Category::Movement) {}

void AutoSprint::Save(Config& c) {
    c.SetBool("sprint.enabled", Enabled());
    c.SetInt("sprint.key", Key());
    c.SetBool("sprint.onlyForward", onlyForward_);
    c.SetBool("sprint.noSneak", noSneak_);
    c.SetBool("sprint.noFood", noFoodCheck_);
    c.SetInt("sprint.engageMin", engageMinMs_);
    c.SetInt("sprint.engageMax", engageMaxMs_);
}

void AutoSprint::Load(const Config& c) {
    SetEnabled(c.GetBool("sprint.enabled", false));
    SetKey(c.GetInt("sprint.key", 0));
    onlyForward_ = c.GetBool("sprint.onlyForward", true);
    noSneak_ = c.GetBool("sprint.noSneak", true);
    noFoodCheck_ = c.GetBool("sprint.noFood", false);
    engageMinMs_ = c.GetInt("sprint.engageMin", 0);
    engageMaxMs_ = c.GetInt("sprint.engageMax", 250);
}

void AutoSprint::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) {
        engaged_ = false;
        mc::SetKeySprint(false);
        return;
    }
    if (snap.hasScreen) {
        engaged_ = false;
        mc::SetKeySprint(false);
        return;
    }

    float fwd = mc::GetForwardImpulse();
    bool canSprint = fwd > 0.01f;
    if (onlyForward_ && std::fabs(mc::GetLeftImpulse()) > 0.01f) canSprint = false;
    if (noSneak_ && mc::IsKeySneakDown()) canSprint = false;
    if (!noFoodCheck_ && mc::GetFoodLevel() <= 6.0) canSprint = false;

    if (canSprint) {
        if (!engaged_) {
            engaged_ = true;
            engageDelay_ = Rng::Range(engageMinMs_, engageMaxMs_);
            engageTimer_.Reset();
        }
        if (engageTimer_.Elapsed(engageDelay_)) mc::SetKeySprint(true);
    } else {
        engaged_ = false;
        mc::SetKeySprint(false);
    }
}

void AutoSprint::DrawSettings() {
    gui::Checkbox("Only straight forward", &onlyForward_);
    gui::Checkbox("Stop while sneaking", &noSneak_);
    gui::Checkbox("Ignore food check", &noFoodCheck_);
    gui::Separator();
    gui::Section("HUMANIZE");
    gui::SliderInt("Engage delay min ms", &engageMinMs_, 0, 1000);
    gui::SliderInt("Engage delay max ms", &engageMaxMs_, 0, 1000);
    gui::Help("Random delay before sprint engages");
}

}  // namespace summer
