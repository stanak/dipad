#include "input8_wrapper.h"

#include "com_provider.h"
#include "logger.h"

namespace dipad {

template <bool W>
Input8WrapperT<W>::Input8WrapperT(typename T::Iface* real, const Config& cfg)
    : m_real(real), m_ref(1), m_cfg(cfg) {
    AddRefServer();
}

template <bool W>
Input8WrapperT<W>::~Input8WrapperT() {
    if (m_real) m_real->Release();
    ReleaseServer();
}

// ----- IUnknown ------------------------------------------------------------

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;

    const IID& selfIid = W ? IID_IDirectInput8W : IID_IDirectInput8A;
    if (riid == IID_IUnknown || riid == selfIid) {
        AddRef();
        *ppvObj = static_cast<typename T::Iface*>(this);
        return S_OK;
    }

    void* raw = nullptr;
    HRESULT hr = m_real->QueryInterface(riid, &raw);
    if (FAILED(hr)) {
        *ppvObj = nullptr;
        return hr;
    }
    Log("Input8: QueryInterface fell through to real for unknown IID");
    *ppvObj = raw;
    return hr;
}

template <bool W>
ULONG __stdcall Input8WrapperT<W>::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

template <bool W>
ULONG __stdcall Input8WrapperT<W>::Release() {
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0) {
        delete this;
    }
    return static_cast<ULONG>(r);
}

// ----- IDirectInput8 -------------------------------------------------------

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::CreateDevice(REFGUID rguid,
                                                  typename T::LpDevice* lplpDirectInputDevice,
                                                  LPUNKNOWN pUnkOuter) {
    if (!lplpDirectInputDevice) return E_POINTER;
    *lplpDirectInputDevice = nullptr;

    typename T::LpDevice realDev = nullptr;
    HRESULT hr = m_real->CreateDevice(rguid, &realDev, pUnkOuter);
    if (FAILED(hr) || !realDev) {
        return hr;
    }

    // Wrap the real device so we can intercept GetDeviceState.
    *lplpDirectInputDevice = new Device8WrapperT<W>(realDev, m_cfg);
    Log("Input8: CreateDevice wrapped (W=%d)", static_cast<int>(W));
    return hr;
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::EnumDevices(DWORD dwDevType,
                                                 typename T::EnumDevicesCB cb,
                                                 LPVOID pvRef, DWORD dwFlags) {
    return m_real->EnumDevices(dwDevType, cb, pvRef, dwFlags);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::GetDeviceStatus(REFGUID rguidInstance) {
    return m_real->GetDeviceStatus(rguidInstance);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::RunControlPanel(HWND hwndOwner, DWORD dwFlags) {
    return m_real->RunControlPanel(hwndOwner, dwFlags);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::Initialize(HINSTANCE hinst, DWORD dwVersion) {
    return m_real->Initialize(hinst, dwVersion);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::FindDevice(REFGUID rguidClass, typename T::LpCStr ptszName,
                                                LPGUID pguidInstance) {
    return m_real->FindDevice(rguidClass, ptszName, pguidInstance);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::EnumDevicesBySemantics(
    typename T::LpCStr ptszUserName, typename T::LpActionFormat lpdiActionFormat,
    typename T::SemanticsCB cb, LPVOID pvRef, DWORD dwFlags) {
    return m_real->EnumDevicesBySemantics(ptszUserName, lpdiActionFormat, cb, pvRef, dwFlags);
}

template <bool W>
HRESULT __stdcall Input8WrapperT<W>::ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK cb,
                                                      typename T::LpConfigParams lpdiCDParams,
                                                      DWORD dwFlags, LPVOID pvRefData) {
    return m_real->ConfigureDevices(cb, lpdiCDParams, dwFlags, pvRefData);
}

template class Input8WrapperT<false>;
template class Input8WrapperT<true>;

} // namespace dipad
