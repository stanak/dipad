// dipad — Side-by-side COM provider for CLSID_DirectInput8 that wraps the real
// implementation so older DirectInput games receive a POV → lX/lY conversion
// transparently. See README.md for the design rationale and installation.

#include "com_provider.h"
#include "config.h"
#include "input8_wrapper.h"
#include "logger.h"
#include "real_dinput8.h"
#include "version.h"

#include <mutex>
#include <windows.h>
#include <strsafe.h>
#include <shlobj.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

namespace {

std::once_flag g_initOnce;
Config         g_config;

// First-touch initialization. We deliberately do NOT load the real dinput8.dll
// from DllMain because doing so under the loader lock can deadlock. Every
// exported entry calls this and the real DLL is loaded on demand.
void EnsureInit() {
    std::call_once(g_initOnce, []() {
        g_config = LoadConfig();
        InitLogger(g_config.enableLog);
        Log("%s %s initialized (PovMode=%d AxisValue=%ld Deadzone=%d PovIndex=%d DebugAxisDump=%d)",
            DIPAD_NAME, DIPAD_VERSION,
            static_cast<int>(g_config.povMode),
            g_config.axisValue, g_config.stickDeadzone, g_config.povIndex,
            static_cast<int>(g_config.debugAxisDump));
        Log("AxisToButton mappings loaded: %zu", g_config.axisButtons.size());
        for (size_t i = 0; i < g_config.axisButtons.size(); ++i) {
            const auto& ab = g_config.axisButtons[i];
            const char* axisName = "?";
            switch (ab.axis) {
            case Axis::X:       axisName = "x";        break;
            case Axis::Y:       axisName = "y";        break;
            case Axis::Z:       axisName = "z";        break;
            case Axis::Rx:      axisName = "rx";       break;
            case Axis::Ry:      axisName = "ry";       break;
            case Axis::Rz:      axisName = "rz";       break;
            case Axis::Slider0: axisName = "slider0";  break;
            case Axis::Slider1: axisName = "slider1";  break;
            }
            Log("  AxisButton[%zu]: %s %s threshold=%ld -> button %d",
                i, axisName, (ab.sign >= 0 ? "+" : "-"), ab.threshold, ab.button);
        }
    });
}

bool IsKnownClsid(REFCLSID rclsid) {
    return IsEqualCLSID(rclsid, CLSID_DirectInput8);
}

// Write a one-line "dipad.dll loaded" marker to %LOCALAPPDATA%\dipad\dipad_load.log.
// Uses only Win32 file APIs (safe under the loader lock) so we can distinguish
// "the DLL was never loaded" from "the DLL was loaded but DllGetClassObject
// was never called" — those need very different fixes.
//
// We deliberately do NOT log next to the DLL itself: many games live under
// C:\Program Files (x86)\ which is read-only for non-elevated processes, and
// modern manifests disable the UAC VirtualStore redirect. LOCALAPPDATA is
// always writable for the current user and predictable for support.
void WriteLoadMarker(HMODULE /*hSelf*/) {
    wchar_t base[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base))) return;

    wchar_t logPath[MAX_PATH] = {};
    if (FAILED(StringCchCopyW(logPath, MAX_PATH, base))) return;
    if (FAILED(StringCchCatW (logPath, MAX_PATH, L"\\dipad"))) return;
    CreateDirectoryW(logPath, nullptr);
    if (FAILED(StringCchCatW (logPath, MAX_PATH, L"\\dipad_load.log"))) return;

    HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    wchar_t hostExe[MAX_PATH] = {};
    DWORD hostLen = GetModuleFileNameW(nullptr, hostExe, MAX_PATH);
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t line[MAX_PATH + 128] = {};
    int written = wsprintfW(line,
        L"[%04u-%02u-%02u %02u:%02u:%02u] dipad.dll loaded into PID %lu (%s)\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        GetCurrentProcessId(),
        (hostLen > 0 ? hostExe : L"<unknown host>"));

    if (written > 0) {
        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD bytes = 0;
        WriteFile(hFile, line, static_cast<DWORD>(written) * sizeof(wchar_t), &bytes, nullptr);
    }
    CloseHandle(hFile);
}

} // namespace

} // namespace dipad

// ===========================================================================
// COM entry points
// ===========================================================================
//
// Exports are declared in src/exports.def (undecorated names on x86). We
// expose only DllGetClassObject and DllCanUnloadNow — the canonical COM
// in-proc server surface required by the side-by-side activation context.

extern "C" HRESULT WINAPI
DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    dipad::EnsureInit();
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (!dipad::IsKnownClsid(rclsid)) {
        // Activation context should never route an unrelated CLSID to us,
        // but be defensive: forward to the real dinput8.dll so the system
        // can at least try to satisfy the request.
        dipad::Log("DllGetClassObject: unexpected CLSID, forwarding to real dinput8.dll");
        return dipad::GetRealClassFactory(rclsid, riid, ppv);
    }

    // Caller wants an IClassFactory for CLSID_DirectInput8 (this is what COM
    // activation always asks for). Return our wrapper factory.
    auto* cf = new dipad::DirectInput8ClassFactory(dipad::g_config);
    HRESULT hr = cf->QueryInterface(riid, ppv);
    cf->Release();
    return hr;
}

extern "C" HRESULT WINAPI
DllCanUnloadNow(void) {
    dipad::EnsureInit();
    // Honor outstanding wrappers + class factories. Once everything is
    // released, allow the loader to unload us.
    return (dipad::GetServerLockCount() == 0) ? S_OK : S_FALSE;
}

// ===========================================================================
// DllMain
// ===========================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        // Drop a single line into dipad_load.log so the user can confirm the
        // DLL physically reached the target process even when DllGetClassObject
        // is never called (e.g. games that resolve dinput8.dll via LoadLibrary
        // instead of CoCreateInstance). Uses only loader-lock-safe APIs.
        dipad::WriteLoadMarker(hModule);
        // Heavy initialization (config load, real dinput8.dll LoadLibrary)
        // is deferred until the first exported entry call, to avoid loader
        // lock issues.
        break;
    case DLL_PROCESS_DETACH:
        dipad::Log("DLL_PROCESS_DETACH (lockCount=%ld)", dipad::GetServerLockCount());
        dipad::ShutdownLogger();
        break;
    default:
        break;
    }
    return TRUE;
}
