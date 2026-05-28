#include "com_provider.h"

#include "input8_wrapper.h"
#include "logger.h"
#include "real_dinput8.h"

namespace dipad {

namespace {

LONG g_serverLocks = 0;

} // namespace

void AddRefServer() {
    InterlockedIncrement(&g_serverLocks);
}

void ReleaseServer() {
    InterlockedDecrement(&g_serverLocks);
}

LONG GetServerLockCount() {
    return g_serverLocks;
}

// ---------------------------------------------------------------------------
// DirectInput8ClassFactory
// ---------------------------------------------------------------------------

DirectInput8ClassFactory::DirectInput8ClassFactory(const Config& cfg)
    : m_ref(1), m_cfg(cfg) {
    AddRefServer();
}

DirectInput8ClassFactory::~DirectInput8ClassFactory() {
    ReleaseServer();
}

HRESULT __stdcall DirectInput8ClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        AddRef();
        *ppvObj = static_cast<IClassFactory*>(this);
        return S_OK;
    }
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

ULONG __stdcall DirectInput8ClassFactory::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

ULONG __stdcall DirectInput8ClassFactory::Release() {
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
}

HRESULT __stdcall DirectInput8ClassFactory::CreateInstance(IUnknown* pUnkOuter,
                                                           REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;

    if (pUnkOuter) {
        // We do not support aggregation. Standard COM behavior.
        return CLASS_E_NOAGGREGATION;
    }

    // Step 1: get a REAL IClassFactory from the genuine dinput8.dll. This
    // bypasses the activation-context override that points back to us.
    IClassFactory* realCF = nullptr;
    HRESULT hr = GetRealClassFactory(CLSID_DirectInput8, IID_IClassFactory, (void**)&realCF);
    if (FAILED(hr) || !realCF) {
        Log("ClassFactory: GetRealClassFactory failed hr=0x%08lx", hr);
        return hr == S_OK ? E_FAIL : hr;
    }

    // Step 2: ask the real CF to create the actual interface. We use the
    // explicit ANSI/Wide IID so the real implementation gives us the right
    // vtable, regardless of which one the caller requested. We then wrap and
    // hand back the wrapped pointer.
    HRESULT outHr = E_NOINTERFACE;

    if (riid == IID_IDirectInput8A || riid == IID_IUnknown) {
        IDirectInput8A* realDI8 = nullptr;
        hr = realCF->CreateInstance(nullptr, IID_IDirectInput8A, (void**)&realDI8);
        if (SUCCEEDED(hr) && realDI8) {
            auto* wrapped = new Input8WrapperA(realDI8, m_cfg);
            *ppvObject = static_cast<IDirectInput8A*>(wrapped);
            Log("ClassFactory: created wrapped IDirectInput8A");
            outHr = S_OK;
        } else {
            Log("ClassFactory: real CreateInstance(IDirectInput8A) failed hr=0x%08lx", hr);
            outHr = hr;
        }
    } else if (riid == IID_IDirectInput8W) {
        IDirectInput8W* realDI8 = nullptr;
        hr = realCF->CreateInstance(nullptr, IID_IDirectInput8W, (void**)&realDI8);
        if (SUCCEEDED(hr) && realDI8) {
            auto* wrapped = new Input8WrapperW(realDI8, m_cfg);
            *ppvObject = static_cast<IDirectInput8W*>(wrapped);
            Log("ClassFactory: created wrapped IDirectInput8W");
            outHr = S_OK;
        } else {
            Log("ClassFactory: real CreateInstance(IDirectInput8W) failed hr=0x%08lx", hr);
            outHr = hr;
        }
    } else {
        // Caller asked for an interface we cannot transparently wrap. Pass
        // through the real interface so we do not break behavior.
        void* raw = nullptr;
        hr = realCF->CreateInstance(nullptr, riid, &raw);
        if (SUCCEEDED(hr)) {
            *ppvObject = raw;
            Log("ClassFactory: passthrough for unknown IID");
            outHr = S_OK;
        } else {
            outHr = hr;
        }
    }

    realCF->Release();
    return outHr;
}

HRESULT __stdcall DirectInput8ClassFactory::LockServer(BOOL fLock) {
    if (fLock) AddRefServer();
    else       ReleaseServer();
    return S_OK;
}

} // namespace dipad
