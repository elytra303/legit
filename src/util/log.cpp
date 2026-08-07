#include "log.h"

#include <direct.h>
#include <shlobj.h>
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace summer {

static FILE* g_log = nullptr;
static std::string g_dir;

std::string ConfigDir() {
    if (!g_dir.empty()) return g_dir;
    char buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        g_dir = std::string(buf) + "\\SummerClient";
    } else {
        g_dir = ".";
    }
    return g_dir;
}

void LogInit() {
    if (g_log) return;
    std::string dir = ConfigDir();
    _mkdir(dir.c_str());
    std::string p = dir + "\\summer.log";
    g_log = fopen(p.c_str(), "a");
    if (g_log) fprintf(g_log, "\n=== SummerClient loaded ===\n");
}

static void LogV(const char* level, const char* fmt, va_list ap) {
    LogInit();
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    if (g_log) {
        fprintf(g_log, "[%s] %s\n", level, buf);
        fflush(g_log);
    }
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV("+", fmt, ap);
    va_end(ap);
}

void LogWarn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV("!", fmt, ap);
    va_end(ap);
}

}  // namespace summer
