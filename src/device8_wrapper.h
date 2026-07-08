#pragma once

#include "config.h"
#include "remap.h"

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

// Traits selecting between the ANSI and Wide-character variants of the
// DirectInput8 device interface and its associated parameter types.
template <bool W>
struct DeviceTraits;

template <>
struct DeviceTraits<false> {
    using Iface             = IDirectInputDevice8A;
    using Instance          = DIDEVICEINSTANCEA;
    using LpInstance        = LPDIDEVICEINSTANCEA;
    using LpObjectInstance  = LPDIDEVICEOBJECTINSTANCEA;
    using EnumObjectsCB     = LPDIENUMDEVICEOBJECTSCALLBACKA;
    using EnumEffectsCB     = LPDIENUMEFFECTSCALLBACKA;
    using LpEffectInfo      = LPDIEFFECTINFOA;
    using LpActionFormat    = LPDIACTIONFORMATA;
    using LpImageInfoHeader = LPDIDEVICEIMAGEINFOHEADERA;
    using LpCStr            = LPCSTR;
};

template <>
struct DeviceTraits<true> {
    using Iface             = IDirectInputDevice8W;
    using Instance          = DIDEVICEINSTANCEW;
    using LpInstance        = LPDIDEVICEINSTANCEW;
    using LpObjectInstance  = LPDIDEVICEOBJECTINSTANCEW;
    using EnumObjectsCB     = LPDIENUMDEVICEOBJECTSCALLBACKW;
    using EnumEffectsCB     = LPDIENUMEFFECTSCALLBACKW;
    using LpEffectInfo      = LPDIEFFECTINFOW;
    using LpActionFormat    = LPDIACTIONFORMATW;
    using LpImageInfoHeader = LPDIDEVICEIMAGEINFOHEADERW;
    using LpCStr            = LPCWSTR;
};

template <bool W>
class Device8WrapperT final : public DeviceTraits<W>::Iface {
public:
    using T = DeviceTraits<W>;

    explicit Device8WrapperT(typename T::Iface* real, const Config& cfg);

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // IDirectInputDevice8
    HRESULT __stdcall GetCapabilities(LPDIDEVCAPS p) override;
    HRESULT __stdcall EnumObjects(typename T::EnumObjectsCB cb, LPVOID ref, DWORD flags) override;
    HRESULT __stdcall GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) override;
    HRESULT __stdcall SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) override;
    HRESULT __stdcall Acquire() override;
    HRESULT __stdcall Unacquire() override;
    HRESULT __stdcall GetDeviceState(DWORD cbData, LPVOID lpvData) override;
    HRESULT __stdcall GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod,
                                    LPDWORD pdwInOut, DWORD dwFlags) override;
    HRESULT __stdcall SetDataFormat(LPCDIDATAFORMAT lpdf) override;
    HRESULT __stdcall SetEventNotification(HANDLE hEvent) override;
    HRESULT __stdcall SetCooperativeLevel(HWND hwnd, DWORD dwFlags) override;
    HRESULT __stdcall GetObjectInfo(typename T::LpObjectInstance pdidoi,
                                    DWORD dwObj, DWORD dwHow) override;
    HRESULT __stdcall GetDeviceInfo(typename T::LpInstance pdidi) override;
    HRESULT __stdcall RunControlPanel(HWND hwndOwner, DWORD dwFlags) override;
    HRESULT __stdcall Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) override;
    HRESULT __stdcall CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff,
                                   LPDIRECTINPUTEFFECT* ppdeff, LPUNKNOWN punkOuter) override;
    HRESULT __stdcall EnumEffects(typename T::EnumEffectsCB cb, LPVOID ref, DWORD type) override;
    HRESULT __stdcall GetEffectInfo(typename T::LpEffectInfo pdei, REFGUID rguid) override;
    HRESULT __stdcall GetForceFeedbackState(LPDWORD pdwOut) override;
    HRESULT __stdcall SendForceFeedbackCommand(DWORD dwFlags) override;
    HRESULT __stdcall EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK cb,
                                               LPVOID ref, DWORD flags) override;
    HRESULT __stdcall Escape(LPDIEFFESCAPE pesc) override;
    HRESULT __stdcall Poll() override;
    HRESULT __stdcall SendDeviceData(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod,
                                     LPDWORD pdwInOut, DWORD fl) override;
    HRESULT __stdcall EnumEffectsInFile(typename T::LpCStr lptszFileName,
                                        LPDIENUMEFFECTSINFILECALLBACK cb,
                                        LPVOID ref, DWORD flags) override;
    HRESULT __stdcall WriteEffectToFile(typename T::LpCStr lptszFileName, DWORD dwEntries,
                                        LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags) override;
    HRESULT __stdcall BuildActionMap(typename T::LpActionFormat lpdiaf,
                                     typename T::LpCStr lpszUserName, DWORD dwFlags) override;
    HRESULT __stdcall SetActionMap(typename T::LpActionFormat lpdiaf,
                                   typename T::LpCStr lpszUserName, DWORD dwFlags) override;
    HRESULT __stdcall GetImageInfo(typename T::LpImageInfoHeader lpdiDevImageInfoHeader) override;

private:
    ~Device8WrapperT();

    typename T::Iface* m_real;
    LONG               m_ref;
    Config             m_cfg;

    // Captured via SetDataFormat: where the axes / POVs / buttons live inside
    // the application's state buffer (works for both the standard joystick
    // formats and custom DIDATAFORMATs), plus whether the underlying device
    // is a game controller at all (so mouse/keyboard devices that happen to
    // use axis-shaped formats are left untouched).
    FormatMap m_map;
    bool      m_isGameController = false;

    // Captured via SetProperty(DIPROP_RANGE) for axis X and Y so the POV
    // synthesized values match the game's chosen range.
    AxisRange m_xRange;
    AxisRange m_yRange;

    // Debug helper for resolving which axis a trigger lives on.
    AxisDebugDumper m_debug;
};

using Device8WrapperA = Device8WrapperT<false>;
using Device8WrapperW = Device8WrapperT<true>;

} // namespace dipad
