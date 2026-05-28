#pragma once

#include "config.h"
#include "device8_wrapper.h"

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

template <bool W>
struct Input8Traits;

template <>
struct Input8Traits<false> {
    using Iface          = IDirectInput8A;
    using LpDevice       = LPDIRECTINPUTDEVICE8A;
    using EnumDevicesCB  = LPDIENUMDEVICESCALLBACKA;
    using LpCStr         = LPCSTR;
    using LpActionFormat = LPDIACTIONFORMATA;
    using SemanticsCB    = LPDIENUMDEVICESBYSEMANTICSCBA;
    using LpConfigParams = LPDICONFIGUREDEVICESPARAMSA;
};

template <>
struct Input8Traits<true> {
    using Iface          = IDirectInput8W;
    using LpDevice       = LPDIRECTINPUTDEVICE8W;
    using EnumDevicesCB  = LPDIENUMDEVICESCALLBACKW;
    using LpCStr         = LPCWSTR;
    using LpActionFormat = LPDIACTIONFORMATW;
    using SemanticsCB    = LPDIENUMDEVICESBYSEMANTICSCBW;
    using LpConfigParams = LPDICONFIGUREDEVICESPARAMSW;
};

template <bool W>
class Input8WrapperT final : public Input8Traits<W>::Iface {
public:
    using T = Input8Traits<W>;

    explicit Input8WrapperT(typename T::Iface* real, const Config& cfg);

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // IDirectInput8
    HRESULT __stdcall CreateDevice(REFGUID rguid, typename T::LpDevice* lplpDirectInputDevice,
                                   LPUNKNOWN pUnkOuter) override;
    HRESULT __stdcall EnumDevices(DWORD dwDevType, typename T::EnumDevicesCB cb,
                                  LPVOID pvRef, DWORD dwFlags) override;
    HRESULT __stdcall GetDeviceStatus(REFGUID rguidInstance) override;
    HRESULT __stdcall RunControlPanel(HWND hwndOwner, DWORD dwFlags) override;
    HRESULT __stdcall Initialize(HINSTANCE hinst, DWORD dwVersion) override;
    HRESULT __stdcall FindDevice(REFGUID rguidClass, typename T::LpCStr ptszName,
                                 LPGUID pguidInstance) override;
    HRESULT __stdcall EnumDevicesBySemantics(typename T::LpCStr ptszUserName,
                                             typename T::LpActionFormat lpdiActionFormat,
                                             typename T::SemanticsCB cb,
                                             LPVOID pvRef, DWORD dwFlags) override;
    HRESULT __stdcall ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK cb,
                                       typename T::LpConfigParams lpdiCDParams,
                                       DWORD dwFlags, LPVOID pvRefData) override;

private:
    ~Input8WrapperT();

    typename T::Iface* m_real;
    LONG               m_ref;
    Config             m_cfg;
};

using Input8WrapperA = Input8WrapperT<false>;
using Input8WrapperW = Input8WrapperT<true>;

} // namespace dipad
