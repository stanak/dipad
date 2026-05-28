#pragma once

#include "config.h"

#include <windows.h>
#include <unknwn.h>

namespace dipad {

// Process-wide lock count consulted by DllCanUnloadNow. Every wrapped object
// (class factory + Input8/Device8 wrappers) increments this on construction
// and decrements on destruction so that COM never tries to unload us while
// state is still live.
void   AddRefServer();
void   ReleaseServer();
LONG   GetServerLockCount();

// IClassFactory implementation that produces wrapped IDirectInput8A/W
// instances. Internally it calls into the real dinput8.dll via
// real_dinput8::GetRealClassFactory to obtain a genuine implementation,
// then wraps it.
class DirectInput8ClassFactory final : public IClassFactory {
public:
    explicit DirectInput8ClassFactory(const Config& cfg);

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // IClassFactory
    HRESULT __stdcall CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    HRESULT __stdcall LockServer(BOOL fLock) override;

private:
    ~DirectInput8ClassFactory();

    LONG   m_ref;
    Config m_cfg;
};

} // namespace dipad
