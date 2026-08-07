#pragma once

#include <vector>

#include "mc/types.h"
#include "module.h"

namespace summer {

class Client {
public:
    static Client& Instance();

    void Initialize();
    void Shutdown();

    void AddModule(Module* m);
    const std::vector<Module*>& Modules() const { return modules_; }
    template <typename T>
    T* Get() {
        for (auto* m : modules_)
            if (auto* t = dynamic_cast<T*>(m)) return t;
        return nullptr;
    }

    void OnFrame();  // capture world + run enabled modules

    bool MenuOpen() const { return menuOpen_; }
    void SetMenuOpen(bool v) { menuOpen_ = v; }

    bool IsInGame() const { return inGame_; }
    mc::WorldSnapshot& Snapshot() { return snapshot_; }

    void SaveConfig();
    void LoadConfig();

private:
    std::vector<Module*> modules_;
    bool menuOpen_ = false;
    bool inGame_ = false;
    mc::WorldSnapshot snapshot_;
};

}  // namespace summer
