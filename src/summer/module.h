#pragma once

#include "config.h"

namespace summer {

enum class Category {
    Combat = 0,
    Visual,
    Movement,
    Misc,
    Count
};

const char* CategoryName(Category c);

class Module {
public:
    Module(const char* name, const char* desc, Category cat)
        : name_(name), desc_(desc), cat_(cat) {}
    virtual ~Module() = default;

    virtual void OnFrame() {}   // per-frame game logic (combat/movement)
    virtual void OnRender() {}  // per-frame overlay drawing (esp/visuals)
    virtual void OnEnable() {}
    virtual void OnDisable() {}
    virtual void DrawSettings() {}
    virtual void Save(Config& cfg) {}
    virtual void Load(const Config& cfg) {}

    const char* Name() const { return name_; }
    const char* Desc() const { return desc_; }
    Category Cat() const { return cat_; }

    bool Enabled() const { return enabled_; }
    void Toggle();
    void SetEnabled(bool on);

    int Key() const { return key_; }
    void SetKey(int k) { key_ = k; }

protected:
    const char* name_;
    const char* desc_;
    Category cat_;
    bool enabled_ = false;
    int key_ = 0;
};

}  // namespace summer
