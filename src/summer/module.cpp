#include "module.h"

#include "util/log.h"

namespace summer {

const char* CategoryName(Category c) {
    switch (c) {
        case Category::Combat:
            return "COMBAT";
        case Category::Visual:
            return "VISUALS";
        case Category::Movement:
            return "MOVEMENT";
        case Category::Misc:
            return "MISC";
        default:
            return "?";
    }
}

void Module::Toggle() {
    if (enabled_) {
        enabled_ = false;
        OnDisable();
        Log("[Module] %s disabled", name_);
    } else {
        enabled_ = true;
        OnEnable();
        Log("[Module] %s enabled", name_);
    }
}

void Module::SetEnabled(bool on) {
    if (on == enabled_) return;
    Toggle();
}

}  // namespace summer
