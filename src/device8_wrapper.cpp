#include "device8_wrapper.h"

#include "com_provider.h"
#include "logger.h"
#include "remap.h"

namespace dipad {

template <bool W>
Device8WrapperT<W>::Device8WrapperT(typename T::Iface* real, const Config& cfg)
    : m_real(real), m_ref(1), m_cfg(cfg) {
    AddRefServer();
}

template <bool W>
Device8WrapperT<W>::~Device8WrapperT() {
    if (m_real) m_real->Release();
    ReleaseServer();
}

// ----- IUnknown ------------------------------------------------------------

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;

    // Hand out our own wrapper for the matching IDirectInputDevice8 IID and
    // the generic IUnknown. For anything else, let the real device decide
    // (we cannot transparently wrap unknown interfaces).
    const IID& selfIid = W ? IID_IDirectInputDevice8W : IID_IDirectInputDevice8A;

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
    Log("Device8: QueryInterface fell through to real for unknown IID");
    *ppvObj = raw;
    return hr;
}

template <bool W>
ULONG __stdcall Device8WrapperT<W>::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

template <bool W>
ULONG __stdcall Device8WrapperT<W>::Release() {
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0) {
        delete this;
    }
    return static_cast<ULONG>(r);
}

// ----- Pass-through methods ------------------------------------------------

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetCapabilities(LPDIDEVCAPS p) {
    return m_real->GetCapabilities(p);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::EnumObjects(typename T::EnumObjectsCB cb, LPVOID ref,
                                                  DWORD flags) {
    return m_real->EnumObjects(cb, ref, flags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) {
    return m_real->GetProperty(rguidProp, pdiph);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) {
    HRESULT hr = m_real->SetProperty(rguidProp, pdiph);

    // DInput property identifiers are "magic pointer" values produced by
    // MAKEDIPROP(integer) — not real GUIDs in memory. Identity must be
    // checked by pointer, not by dereferencing the storage.
    if (SUCCEEDED(hr) && pdiph && &rguidProp == &DIPROP_RANGE &&
        pdiph->dwSize >= sizeof(DIPROPRANGE)) {
        auto* pr = reinterpret_cast<const DIPROPRANGE*>(pdiph);
        const DWORD how   = pr->diph.dwHow;
        const DWORD obj   = pr->diph.dwObj;
        const bool  toAll = (how == DIPH_DEVICE);

        auto matchesAxis = [&](DWORD ofs) {
            return how == DIPH_BYOFFSET && obj == ofs;
        };

        if (toAll || matchesAxis(DIJOFS_X)) {
            m_xRange = {pr->lMin, pr->lMax, true};
        }
        if (toAll || matchesAxis(DIJOFS_Y)) {
            m_yRange = {pr->lMin, pr->lMax, true};
        }
    }
    return hr;
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::Acquire() {
    return m_real->Acquire();
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::Unacquire() {
    return m_real->Unacquire();
}

// ----- The core: GetDeviceState --------------------------------------------

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetDeviceState(DWORD cbData, LPVOID lpvData) {
    HRESULT hr = m_real->GetDeviceState(cbData, lpvData);
    if (SUCCEEDED(hr) && m_isJoystick && lpvData && cbData == m_dataSize) {
        // Order matters:
        //   1. POV → lX/lY (so the host sees both D-pad and stick as motion).
        //   2. Axis → button (so L2/R2-style triggers can be bound as buttons
        //      in games that only understand button input).
        //   3. Debug dump runs LAST and observes the final, post-remap state
        //      so the user can see exactly what the game receives.
        ApplyPovToStick(lpvData, cbData, m_cfg, m_xRange, m_yRange);
        ApplyAxisToButtons(lpvData, cbData, m_cfg);
        if (m_cfg.debugAxisDump) {
            m_debug.Dump(lpvData, cbData);
        }
    }
    return hr;
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetDeviceData(DWORD cbObjectData,
                                                    LPDIDEVICEOBJECTDATA rgdod,
                                                    LPDWORD pdwInOut, DWORD dwFlags) {
    // Buffered mode is left alone — most older joystick-using games poll via
    // GetDeviceState. Buffered POV events will still flow through, but they
    // will not be mirrored onto lX/lY events. See README for limitations.
    return m_real->GetDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SetDataFormat(LPCDIDATAFORMAT lpdf) {
    HRESULT hr = m_real->SetDataFormat(lpdf);
    if (SUCCEEDED(hr) && lpdf) {
        m_dataSize = lpdf->dwDataSize;
        m_isJoystick =
            (lpdf->dwDataSize == sizeof(DIJOYSTATE) ||
             lpdf->dwDataSize == sizeof(DIJOYSTATE2));
        Log("Device8: SetDataFormat dwDataSize=%lu isJoystick=%d",
            static_cast<unsigned long>(lpdf->dwDataSize),
            static_cast<int>(m_isJoystick));
    }
    return hr;
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SetEventNotification(HANDLE hEvent) {
    return m_real->SetEventNotification(hEvent);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SetCooperativeLevel(HWND hwnd, DWORD dwFlags) {
    return m_real->SetCooperativeLevel(hwnd, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetObjectInfo(typename T::LpObjectInstance pdidoi,
                                                    DWORD dwObj, DWORD dwHow) {
    return m_real->GetObjectInfo(pdidoi, dwObj, dwHow);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetDeviceInfo(typename T::LpInstance pdidi) {
    return m_real->GetDeviceInfo(pdidi);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::RunControlPanel(HWND hwndOwner, DWORD dwFlags) {
    return m_real->RunControlPanel(hwndOwner, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) {
    return m_real->Initialize(hinst, dwVersion, rguid);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff,
                                                   LPDIRECTINPUTEFFECT* ppdeff,
                                                   LPUNKNOWN punkOuter) {
    return m_real->CreateEffect(rguid, lpeff, ppdeff, punkOuter);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::EnumEffects(typename T::EnumEffectsCB cb,
                                                  LPVOID ref, DWORD type) {
    return m_real->EnumEffects(cb, ref, type);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetEffectInfo(typename T::LpEffectInfo pdei, REFGUID rguid) {
    return m_real->GetEffectInfo(pdei, rguid);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetForceFeedbackState(LPDWORD pdwOut) {
    return m_real->GetForceFeedbackState(pdwOut);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SendForceFeedbackCommand(DWORD dwFlags) {
    return m_real->SendForceFeedbackCommand(dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::EnumCreatedEffectObjects(
    LPDIENUMCREATEDEFFECTOBJECTSCALLBACK cb, LPVOID ref, DWORD flags) {
    return m_real->EnumCreatedEffectObjects(cb, ref, flags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::Escape(LPDIEFFESCAPE pesc) {
    return m_real->Escape(pesc);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::Poll() {
    return m_real->Poll();
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SendDeviceData(DWORD cbObjectData,
                                                     LPCDIDEVICEOBJECTDATA rgdod,
                                                     LPDWORD pdwInOut, DWORD fl) {
    return m_real->SendDeviceData(cbObjectData, rgdod, pdwInOut, fl);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::EnumEffectsInFile(typename T::LpCStr lptszFileName,
                                                        LPDIENUMEFFECTSINFILECALLBACK cb,
                                                        LPVOID ref, DWORD flags) {
    return m_real->EnumEffectsInFile(lptszFileName, cb, ref, flags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::WriteEffectToFile(typename T::LpCStr lptszFileName,
                                                        DWORD dwEntries,
                                                        LPDIFILEEFFECT rgDiFileEft,
                                                        DWORD dwFlags) {
    return m_real->WriteEffectToFile(lptszFileName, dwEntries, rgDiFileEft, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::BuildActionMap(typename T::LpActionFormat lpdiaf,
                                                     typename T::LpCStr lpszUserName,
                                                     DWORD dwFlags) {
    return m_real->BuildActionMap(lpdiaf, lpszUserName, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::SetActionMap(typename T::LpActionFormat lpdiaf,
                                                   typename T::LpCStr lpszUserName,
                                                   DWORD dwFlags) {
    return m_real->SetActionMap(lpdiaf, lpszUserName, dwFlags);
}

template <bool W>
HRESULT __stdcall Device8WrapperT<W>::GetImageInfo(typename T::LpImageInfoHeader header) {
    return m_real->GetImageInfo(header);
}

// Explicit template instantiations for the ANSI and Wide variants.
template class Device8WrapperT<false>;
template class Device8WrapperT<true>;

} // namespace dipad
