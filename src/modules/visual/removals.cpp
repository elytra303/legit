#include "removals.h"

#include "../../gui/gui.h"
#include "../../mc/minecraft.h"
#include "../../client.h"

namespace summer {

Removals::Removals()
    : Module("Removals", "Removes annoying visual effects (fire, hurt cam, ...)",
             Category::Visual) {}

void Removals::Save(Config& c) {
    c.SetBool("removals.enabled", Enabled());
    c.SetInt("removals.key", Key());
    c.SetBool("removals.fullbright", fullbright_);
    c.SetBool("removals.hurt", noHurtCam_);
    c.SetBool("removals.fire", noFire_);
    c.SetBool("removals.bob", noBob_);
}

void Removals::Load(const Config& c) {
    SetEnabled(c.GetBool("removals.enabled", false));
    SetKey(c.GetInt("removals.key", 0));
    fullbright_ = c.GetBool("removals.fullbright", true);
    noHurtCam_ = c.GetBool("removals.hurt", true);
    noFire_ = c.GetBool("removals.fire", true);
    noBob_ = c.GetBool("removals.bob", false);
}

void Removals::OnDisable() {
    mc::SetGamma(1.0);
    mc::SetBobView(true);
}

void Removals::OnFrame() {
    auto& snap = Client::Instance().Snapshot();
    if (!snap.valid) return;

    if (fullbright_) mc::SetGamma(16.0);
    if (noHurtCam_) mc::SetHurtTime(0);
    if (noFire_) mc::SetFireTicks(0);
    if (noBob_) mc::SetBobView(false);
}

void Removals::DrawSettings() {
    gui::Checkbox("Fullbright", &fullbright_);
    gui::Help("Max gamma - see in the dark without torches");
    gui::Checkbox("No hurt cam", &noHurtCam_);
    gui::Checkbox("No fire overlay", &noFire_);
    gui::Checkbox("No view bobbing", &noBob_);
}

}  // namespace summer
