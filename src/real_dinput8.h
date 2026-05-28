#pragma once

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

using PFN_DllCanUnloadNow    = HRESULT(WINAPI*)(void);
using PFN_DllGetClassObject  = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);

struct RealDInput8 {
    HMODULE                 module             = nullptr;
    PFN_DllCanUnloadNow     DllCanUnloadNow    = nullptr;
    PFN_DllGetClassObject   DllGetClassObject  = nullptr;
};

// Loads the genuine dinput8.dll from %SystemRoot%\System32 (which the WoW64
// redirector maps to SysWOW64 for 32-bit processes). Idempotent and
// thread-safe.
//
// We deliberately avoid CoCreateInstance because the side-by-side activation
// context overrides CLSID_DirectInput8 to our own DLL (that is the whole
// point of dipad), so going through COM activation would recurse straight back
// into us. We get a real IClassFactory by reaching past activation directly
// into the genuine dinput8.dll's DllGetClassObject export.
bool LoadRealDInput8();

const RealDInput8& GetReal();

// Convenience helper: gets a real IClassFactory* for the given CLSID via the
// real dinput8.dll. The returned interface is AddRef'd; caller must Release.
HRESULT GetRealClassFactory(REFCLSID rclsid, REFIID riid, void** out);

} // namespace dipad
