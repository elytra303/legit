#include "clickpearl.h"

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../util/random.h"
#include "../../client.h"

namespace summer {

ClickPearlModule::ClickPearlModule()
    : Module("ClickPearl",
             "Selects an ender pearl and clicks it with human-like timing",
             Category::Combat) {}

void ClickPearlModule::Save(Config& c) {
    c.SetBool("clickpearl.enabled", Enabled());
    c.SetInt("clickpearl.key", Key());
    c.SetInt("clickpearl.mode", mode_);
    c.SetBool("clickpearl.humanize", humanize_);
    c.SetInt("clickpearl.minDelay", minDelay_);
    c.SetInt("clickpearl.maxDelay", maxDelay_);
    c.SetInt("clickpearl.pressMin", pressMin_);
    c.SetInt("clickpearl.pressMax", pressMax_);
}

void ClickPearlModule::Load(const Config& c) {
    SetEnabled(c.GetBool("clickpearl.enabled", false));
    SetKey(c.GetInt("clickpearl.key", 0));
    mode_ = c.GetInt("clickpearl.mode", 0);
    humanize_ = c.GetBool("clickpearl.humanize", true);
    minDelay_ = c.GetInt("clickpearl.minDelay", 190);
    maxDelay_ = c.GetInt("clickpearl.maxDelay", 260);
    pressMin_ = c.GetInt("clickpearl.pressMin", 55);
    pressMax_ = c.GetInt("clickpearl.pressMax", 95);
    if (maxDelay_ < minDelay_) maxDelay_ = minDelay_;
    if (pressMax_ < pressMin_) pressMax_ = pressMin_;
}

double ClickPearlModule::Jitter(double a, double b) {
    return humanize_ ? Rng::Range(a, b) : (a + b) * 0.5;
}

int ClickPearlModule::FindPearlSlot() {
    for (int i = 0; i < 9; ++i) {
        std::string name;
        if (!mc::GetHotbarItemName(i, name)) continue;
        if (name == "item.minecraft.ender_pearl") return i;
    }
    return -1;
}

void ClickPearlModule::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid || snap.hasScreen) {
        clicking_ = false;
        switching_ = false;
        mc::SetKeyUse(false);
        return;
    }

    bool active = (mode_ == 1) || mc::IsKeyUseDown();
    if (!active) {
        clicking_ = false;
        switching_ = false;
        mc::SetKeyUse(false);
        return;
    }

    int pearl = FindPearlSlot();
    if (pearl < 0) {
        clicking_ = false;
        switching_ = false;
        mc::SetKeyUse(false);
        return;
    }

    int cur = mc::GetSelectedSlot();
    if (cur != pearl && !switching_) {
        switching_ = true;
        switchDelay_ = Jitter(35, 90);
        switchTimer_.Reset();
    }
    if (switching_) {
        if (switchTimer_.Elapsed(switchDelay_)) {
            mc::SetSelectedSlot(pearl);
            switching_ = false;
            nextClick_ = Jitter((double)minDelay_, (double)maxDelay_);
            clickTimer_.Reset();
        }
        return;
    }

    if (clicking_) {
        if (clickTimer_.Elapsed(pressDelay_)) {
            mc::SetKeyUse(false);
            clicking_ = false;
            nextClick_ = Jitter((double)minDelay_, (double)maxDelay_);
            clickTimer_.Reset();
        }
        return;
    }
    if (clickTimer_.Elapsed(nextClick_)) {
        mc::SetKeyUse(true);
        clicking_ = true;
        pressDelay_ = Jitter((double)pressMin_, (double)pressMax_);
        clickTimer_.Reset();
    }
}

void ClickPearlModule::DrawSettings() {
    static const char* modes[] = {"While using", "Auto loop"};
    gui::Combo("Mode", &mode_, modes, 2);
    gui::Help("While using: throws only while you hold right-click");
    gui::Separator();
    gui::Section("TIMING");
    gui::SliderInt("Min delay ms", &minDelay_, 60, 800);
    gui::SliderInt("Max delay ms", &maxDelay_, 60, 800);
    gui::SliderInt("Press min ms", &pressMin_, 20, 300);
    gui::SliderInt("Press max ms", &pressMax_, 20, 300);
    gui::Checkbox("Humanize", &humanize_);
    gui::Help("Throws the pearl from the first hotbar slot");
}

}  // namespace summer
