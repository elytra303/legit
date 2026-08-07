#pragma once
#include <string>

namespace summer {
void LogInit();
void Log(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
std::string ConfigDir();
}
