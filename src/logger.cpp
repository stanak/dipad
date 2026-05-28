#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <shlobj.h>

namespace dipad {

namespace {

std::mutex g_logMutex;
FILE* g_logFile = nullptr;
bool g_enabled = false;
std::wstring g_lastLogPath;

// Resolve %LOCALAPPDATA%\dipad and ensure the directory exists. Used as the
// canonical log location. Writing into the game folder is unreliable because
// most games live under C:\Program Files (x86)\ which requires admin to
// write, and asInvoker hosts (like our test_actctx) do not get the UAC
// VirtualStore fallback.
std::wstring GetLocalAppDataDipadDir() {
    wchar_t buf[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf))) {
        return L"";
    }
    std::wstring path(buf);
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += L"dipad";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L'\\';
    return path;
}

std::wstring GetLogPath() {
    auto dir = GetLocalAppDataDipadDir();
    if (dir.empty()) return L"dipad.log";
    return dir + L"dipad.log";
}

} // namespace

std::wstring GetLogDirectory() {
    return GetLocalAppDataDipadDir();
}

void InitLogger(bool enabled) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_enabled = enabled;
    if (!enabled) return;
    if (g_logFile) return;

    auto path = GetLogPath();
    g_lastLogPath = path;
    _wfopen_s(&g_logFile, path.c_str(), L"wb");
    if (!g_logFile) {
        g_enabled = false;
        return;
    }

    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::fprintf(g_logFile,
                 "[%04d-%02d-%02d %02d:%02d:%02d] dipad log opened at %ls\n",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec,
                 path.c_str());
    std::fflush(g_logFile);
}

void ShutdownLogger() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        std::fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void Log(const char* fmt, ...) {
    if (!g_enabled) return;
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logFile) return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_logFile, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_logFile);
    std::fflush(g_logFile);
}

} // namespace dipad
