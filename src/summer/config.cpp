#include "config.h"

#include <direct.h>
#include <windows.h>

#include <fstream>
#include <sstream>

#include "util/log.h"

namespace summer {

Config g_config;

std::string Config::Path() const {
    return ConfigDir() + "\\SummerClient.cfg";
}

void Config::Load() {
    kv_.clear();
    std::ifstream in(Path());
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv_[line.substr(0, eq)] = line.substr(eq + 1);
    }
    Log("[Config] loaded %zu keys", kv_.size());
}

void Config::Save() {
    std::string dir = ConfigDir();
    _mkdir(dir.c_str());
    std::ofstream out(Path(), std::ios::trunc);
    if (!out) {
        LogWarn("[Config] failed to save %s", Path().c_str());
        return;
    }
    for (const auto& [k, v] : kv_) out << k << "=" << v << "\n";
    Log("[Config] saved");
}

void Config::Set(const std::string& key, const std::string& value) {
    kv_[key] = value;
}

std::string Config::Get(const std::string& key, const std::string& def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : it->second;
}

void Config::SetBool(const std::string& key, bool v) {
    Set(key, v ? "1" : "0");
}

bool Config::GetBool(const std::string& key, bool def) const {
    std::string v = Get(key, def ? "1" : "0");
    return v == "1" || v == "true";
}

void Config::SetInt(const std::string& key, int v) {
    Set(key, std::to_string(v));
}

int Config::GetInt(const std::string& key, int def) const {
    std::string v = Get(key, "");
    return v.empty() ? def : std::stoi(v);
}

void Config::SetDouble(const std::string& key, double v) {
    std::ostringstream ss;
    ss << v;
    Set(key, ss.str());
}

double Config::GetDouble(const std::string& key, double def) const {
    std::string v = Get(key, "");
    return v.empty() ? def : std::stod(v);
}

}  // namespace summer
