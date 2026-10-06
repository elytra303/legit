#include "registry.h"

#include "client.h"
#include "modules/combat/hitbox.h"

namespace summer {

void RegisterAllModules(Client& client) {
    client.AddModule(new HitBoxModule());
}

}  // namespace summer
