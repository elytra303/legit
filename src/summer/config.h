#pragma once

#include <map>
#include <string>

namespace summer {

class Config {
public:
    void Load();
    void Save();

    void Set(const std::string& key, const std::string& value);
    std::string Get(const std::string& key, const std::string& def = "") const;

    void SetBool(const std::string& key, bool v);
    bool GetBool(const std::string& key, bool def = false) const;

    void SetInt(const std::string& key, int v);
    int GetInt(const std::string& key, int def = 0) const;

    void SetDouble(const std::string& key, double v);
    double GetDouble(const std::string& key, double def = 0.0) const;

private:
    std::string Path() const;
    std::map<std::string, std::string> kv_;
};

extern Config g_config;

}  // namespace summer
