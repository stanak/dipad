#include "real_dinput8.h"

#include "logger.h"

#include <mutex>
#include <string>

namespace dipad {

namespace {

RealDInput8    g_real;
std::once_flag g_loadOnce;
bool           g_loadOk = false;

template <typename T>
T Resolve(HMODULE mod, const char* name) {
    auto p = reinterpret_cast<T>(GetProcAddress(mod, name));
    if (!p) Log("real_dinput8: GetProcAddress failed for %s", name);
    return p;
}

void DoLoad() {
    wchar_t sysdir[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(sysdir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        Log("real_dinput8: GetSystemDirectoryW failed (err=%lu)", GetLastError());
        return;
    }

    std::wstring path = sysdir;
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += L"dinput8.dll";

    g_real.module = LoadLibraryW(path.c_str());
    if (!g_real.module) {
        Log("real_dinput8: LoadLibraryW(%ls) failed (err=%lu)",
            path.c_str(), GetLastError());
        return;
    }

    g_real.DllCanUnloadNow =
        Resolve<PFN_DllCanUnloadNow>(g_real.module, "DllCanUnloadNow");
    g_real.DllGetClassObject =
        Resolve<PFN_DllGetClassObject>(g_real.module, "DllGetClassObject");
    g_real.DirectInput8Create =
        Resolve<PFN_DirectInput8Create>(g_real.module, "DirectInput8Create");

    g_loadOk = (g_real.DllGetClassObject != nullptr);
    if (g_loadOk) {
        Log("real_dinput8: loaded real dinput8.dll at %ls", path.c_str());
    } else {
        Log("real_dinput8: DllGetClassObject not found in real dinput8.dll");
    }
}

} // namespace

bool LoadRealDInput8() {
    std::call_once(g_loadOnce, DoLoad);
    return g_loadOk;
}

const RealDInput8& GetReal() {
    return g_real;
}

HRESULT GetRealClassFactory(REFCLSID rclsid, REFIID riid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!LoadRealDInput8()) return E_FAIL;
    return g_real.DllGetClassObject(rclsid, riid, out);
}

} // namespace dipad
