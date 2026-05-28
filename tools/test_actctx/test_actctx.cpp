// test_actctx.exe — Reg-Free COM activation context PoC for dipad.
//
// Companion to dipad. Builds a tiny console EXE that does exactly what
// 非想天則 (and many old DI8 games) does at startup:
//
//     CoCreateInstance(CLSID_DirectInput8, ..., IID_IDirectInput8A, ...)
//
// Drop test_actctx.exe + test_actctx.exe.manifest + dipad.dll + dipad.manifest
// into the same folder, run it, and read the output:
//
//   * If dipad.dll appears in the loaded module list and the message
//     "wrapped IDirectInput8A" shows up, the manifest pipeline works.
//   * If only Windows\System32\dinput8.dll appears, the activation
//     context override did not take effect.
//
// This is a debugging tool — not part of the shipped dipad release.

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <objbase.h>
#include <dinput.h>
#include <psapi.h>
#include <shlwapi.h>
#include <cstdio>

namespace {

void PrintHr(const char* what, HRESULT hr) {
    std::printf("[test] %-40s hr = 0x%08lx\n", what, static_cast<unsigned long>(hr));
}

void DumpModulesContaining(const char* needle) {
    std::printf("[test] loaded modules matching '%s':\n", needle);
    HMODULE mods[1024];
    DWORD cbNeeded = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &cbNeeded)) {
        std::printf("[test]   EnumProcessModules failed (err=%lu)\n", GetLastError());
        return;
    }
    DWORD count = cbNeeded / sizeof(HMODULE);
    bool any = false;
    for (DWORD i = 0; i < count; ++i) {
        char path[MAX_PATH] = {};
        if (GetModuleFileNameA(mods[i], path, MAX_PATH)) {
            if (StrStrIA(path, needle)) {
                std::printf("[test]   %s\n", path);
                any = true;
            }
        }
    }
    if (!any) std::printf("[test]   (none)\n");
}

void DumpActivationContext() {
    std::printf("[test] querying current activation context...\n");
    HANDLE hActCtx = INVALID_HANDLE_VALUE;
    if (!GetCurrentActCtx(&hActCtx)) {
        std::printf("[test]   GetCurrentActCtx failed (err=%lu)\n", GetLastError());
        return;
    }
    if (hActCtx == nullptr) {
        std::printf("[test]   NULL handle returned (default context)\n");
        return;
    }
    SIZE_T required = 0;
    QueryActCtxW(0, hActCtx, nullptr,
                 AssemblyDetailedInformationInActivationContext,
                 nullptr, 0, &required);
    std::printf("[test]   activation context handle = %p (detail bytes needed = %llu)\n",
                hActCtx, static_cast<unsigned long long>(required));
    ReleaseActCtx(hActCtx);
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::printf("=== test_actctx ===\n");

    DumpActivationContext();
    DumpModulesContaining("dinput8");
    DumpModulesContaining("dipad");

    HRESULT hr = CoInitialize(nullptr);
    PrintHr("CoInitialize", hr);

    IDirectInput8A* di = nullptr;
    hr = CoCreateInstance(CLSID_DirectInput8,
                          nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_IDirectInput8A,
                          reinterpret_cast<void**>(&di));
    PrintHr("CoCreateInstance(CLSID_DirectInput8)", hr);

    std::printf("\n--- after CoCreateInstance ---\n");
    DumpModulesContaining("dinput8");
    DumpModulesContaining("dipad");

    if (SUCCEEDED(hr) && di) {
        di->Release();
    }
    CoUninitialize();

    std::printf("\nDone. Press Enter to quit.\n");
    std::getchar();
    return 0;
}
