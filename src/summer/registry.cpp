#include "registry.h"

#include "client.h"
#include "modules/combat/autoswap.h"
#include "modules/combat/clickpearl.h"
#include "modules/combat/hitbox.h"
#include "modules/combat/triggerbot.h"
#include "modules/movement/autosprint.h"
#include "modules/movement/elytraswap.h"
#include "modules/movement/screenwalk.h"
#include "modules/visual/esp.h"
#include "modules/visual/hitboxes.h"
#include "modules/visual/removals.h"

namespace summer {

void RegisterAllModules(Client& client) {
    client.AddModule(new HitboxModule());
    client.AddModule(new TriggerBot());
    client.AddModule(new AutoSwap());
    client.AddModule(new ClickPearlModule());
    client.AddModule(new ESPModule());
    client.AddModule(new HitboxesRender());
    client.AddModule(new Removals());
    client.AddModule(new AutoSprint());
    client.AddModule(new ScreenWalk());
    client.AddModule(new ElytraSwapModule());
}

}  // namespace summer
