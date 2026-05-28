#include "remap.h"

#include "logger.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

namespace {

struct Direction {
    int dx = 0;  // -1 left, 0, +1 right
    int dy = 0;  // -1 up,   0, +1 down
};

Direction DecodePov(DWORD pov) {
    Direction d;
    // DInput convention: -1 (LOWORD == 0xFFFF) means centered.
    if (LOWORD(pov) == 0xFFFF) return d;

    // POV is reported in hundredths of degrees. 0 = up, clockwise.
    const int deg = static_cast<int>(pov / 100) % 360;

    if (deg >= 315 || deg <= 45)  d.dy = -1;
    if (deg >= 135 && deg <= 225) d.dy = +1;
    if (deg >= 45  && deg <= 135) d.dx = +1;
    if (deg >= 225 && deg <= 315) d.dx = -1;
    return d;
}

bool StickCentered(LONG x, LONG y, int deadzone) {
    return std::abs(x) <= deadzone && std::abs(y) <= deadzone;
}

LONG AxisPositive(const AxisRange& r, long fallback) {
    return r.set ? r.max : static_cast<LONG>(fallback);
}

LONG AxisNegative(const AxisRange& r, long fallback) {
    return r.set ? r.min : static_cast<LONG>(-fallback);
}

LONG AxisCenter(const AxisRange& r) {
    return r.set ? static_cast<LONG>((r.min + r.max) / 2) : 0;
}

void ApplyOn(LONG& lX, LONG& lY,
             DWORD pov,
             const Config& cfg,
             const AxisRange& xRange,
             const AxisRange& yRange) {
    const Direction dir = DecodePov(pov);
    const bool povActive = (dir.dx != 0 || dir.dy != 0);

    LONG povX = lX;
    LONG povY = lY;
    if (dir.dx > 0) povX = AxisPositive(xRange, cfg.axisValue);
    else if (dir.dx < 0) povX = AxisNegative(xRange, cfg.axisValue);
    else povX = AxisCenter(xRange);
    if (dir.dy > 0) povY = AxisPositive(yRange, cfg.axisValue);
    else if (dir.dy < 0) povY = AxisNegative(yRange, cfg.axisValue);
    else povY = AxisCenter(yRange);

    auto write = [&](LONG& target, LONG src, bool enabled) {
        if (enabled) target = src;
    };

    switch (cfg.povMode) {
    case PovMode::PovOnly:
        write(lX, povX, cfg.overrideLX);
        write(lY, povY, cfg.overrideLY);
        break;

    case PovMode::PovPriority:
        if (povActive) {
            if (dir.dx != 0) write(lX, povX, cfg.overrideLX);
            if (dir.dy != 0) write(lY, povY, cfg.overrideLY);
        }
        break;

    case PovMode::StickPriority:
        if (povActive && StickCentered(lX, lY, cfg.stickDeadzone)) {
            if (dir.dx != 0) write(lX, povX, cfg.overrideLX);
            if (dir.dy != 0) write(lY, povY, cfg.overrideLY);
        }
        break;
    }
}

} // namespace

void ApplyPovToStick(void* buffer,
                     std::size_t cbData,
                     const Config& cfg,
                     const AxisRange& xRange,
                     const AxisRange& yRange) {
    if (!buffer) return;

    const int idx = std::clamp(cfg.povIndex, 0, 3);

    if (cbData == sizeof(DIJOYSTATE)) {
        auto* s = static_cast<DIJOYSTATE*>(buffer);
        ApplyOn(s->lX, s->lY, s->rgdwPOV[idx], cfg, xRange, yRange);
    } else if (cbData == sizeof(DIJOYSTATE2)) {
        auto* s = static_cast<DIJOYSTATE2*>(buffer);
        ApplyOn(s->lX, s->lY, s->rgdwPOV[idx], cfg, xRange, yRange);
    }
    // Any other format (keyboard / mouse / custom) is passed through unchanged.
}

// ---------------------------------------------------------------------------
// Axis → synthetic button (L2/R2 bridge)
// ---------------------------------------------------------------------------

namespace {

template <typename State>
LONG ReadAxis(const State& s, Axis a) {
    switch (a) {
    case Axis::X:       return s.lX;
    case Axis::Y:       return s.lY;
    case Axis::Z:       return s.lZ;
    case Axis::Rx:      return s.lRx;
    case Axis::Ry:      return s.lRy;
    case Axis::Rz:      return s.lRz;
    case Axis::Slider0: return s.rglSlider[0];
    case Axis::Slider1: return s.rglSlider[1];
    }
    return 0;
}

template <typename State, std::size_t Buttons>
void ApplyButtons(State& s, const Config& cfg, BYTE (&buttons)[Buttons]) {
    for (const auto& ab : cfg.axisButtons) {
        const LONG v = ReadAxis(s, ab.axis);
        const bool active = (ab.sign >= 0)
            ? (v >=  ab.threshold)
            : (v <= -ab.threshold);
        if (!active) continue;
        if (ab.button < 0) continue;
        if (static_cast<std::size_t>(ab.button) >= Buttons) continue;
        buttons[ab.button] = 0x80;
    }
}

} // namespace

void ApplyAxisToButtons(void* buffer, std::size_t cbData, const Config& cfg) {
    if (!buffer) return;
    if (cfg.axisButtons.empty()) return;

    if (cbData == sizeof(DIJOYSTATE)) {
        auto* s = static_cast<DIJOYSTATE*>(buffer);
        ApplyButtons(*s, cfg, s->rgbButtons);
    } else if (cbData == sizeof(DIJOYSTATE2)) {
        auto* s = static_cast<DIJOYSTATE2*>(buffer);
        ApplyButtons(*s, cfg, s->rgbButtons);
    }
}

// ---------------------------------------------------------------------------
// AxisDebugDumper
// ---------------------------------------------------------------------------

namespace {

// Common axis fields shared between DIJOYSTATE and DIJOYSTATE2 — both layouts
// place these in the same offsets, so a single helper template works for both.
template <typename State, std::size_t Buttons>
struct StateView {
    long lX, lY, lZ;
    long lRx, lRy, lRz;
    long slider0, slider1;
    unsigned long pov0;
    const BYTE (&buttons)[Buttons];
};

} // namespace

void AxisDebugDumper::Dump(const void* buffer, std::size_t cbData) {
    if (!buffer) return;

    long lX = 0, lY = 0, lZ = 0, lRx = 0, lRy = 0, lRz = 0, s0 = 0, s1 = 0;
    unsigned long pov0 = 0xFFFFFFFFu;

    unsigned long long maskLo = 0, maskHi = 0;
    char btnList[256] = {};
    int  off = 0;

    auto recordButtons = [&](const BYTE* btns, std::size_t n) {
        for (std::size_t i = 0; i < n && i < 64; ++i) {
            if (btns[i] & 0x80) maskLo |= (1ull << i);
        }
        for (std::size_t i = 64; i < n; ++i) {
            if (btns[i] & 0x80) maskHi |= (1ull << (i - 64));
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (btns[i] & 0x80) {
                int w = std::snprintf(btnList + off,
                                      sizeof(btnList) - off,
                                      "%s%zu", off == 0 ? "" : ",", i);
                if (w <= 0 || off + w >= static_cast<int>(sizeof(btnList))) break;
                off += w;
            }
        }
    };

    if (cbData == sizeof(DIJOYSTATE)) {
        auto* s = static_cast<const DIJOYSTATE*>(buffer);
        lX = s->lX; lY = s->lY; lZ = s->lZ;
        lRx = s->lRx; lRy = s->lRy; lRz = s->lRz;
        s0 = s->rglSlider[0]; s1 = s->rglSlider[1];
        pov0 = s->rgdwPOV[0];
        recordButtons(s->rgbButtons, 32);
    } else if (cbData == sizeof(DIJOYSTATE2)) {
        auto* s = static_cast<const DIJOYSTATE2*>(buffer);
        lX = s->lX; lY = s->lY; lZ = s->lZ;
        lRx = s->lRx; lRy = s->lRy; lRz = s->lRz;
        s0 = s->rglSlider[0]; s1 = s->rglSlider[1];
        pov0 = s->rgdwPOV[0];
        recordButtons(s->rgbButtons, 128);
    } else {
        return;
    }

    const bool changed = !m_haveBaseline ||
        lX != m_lastX || lY != m_lastY || lZ != m_lastZ ||
        lRx != m_lastRx || lRy != m_lastRy || lRz != m_lastRz ||
        s0 != m_lastSlider0 || s1 != m_lastSlider1 ||
        pov0 != m_lastPov ||
        maskLo != m_lastButtonMaskLo || maskHi != m_lastButtonMaskHi;
    if (!changed) return;

    if (off == 0) std::snprintf(btnList, sizeof(btnList), "-");

    Log("[axis-dump] X=%ld Y=%ld Z=%ld | Rx=%ld Ry=%ld Rz=%ld | S0=%ld S1=%ld | POV0=0x%08lX | btns=%s",
        lX, lY, lZ, lRx, lRy, lRz, s0, s1, pov0, btnList);

    m_lastX = lX; m_lastY = lY; m_lastZ = lZ;
    m_lastRx = lRx; m_lastRy = lRy; m_lastRz = lRz;
    m_lastSlider0 = s0; m_lastSlider1 = s1;
    m_lastPov = pov0;
    m_lastButtonMaskLo = maskLo;
    m_lastButtonMaskHi = maskHi;
    m_haveBaseline = true;
}

} // namespace dipad
