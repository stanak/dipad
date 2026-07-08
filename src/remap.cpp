#include "remap.h"

#include "logger.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dipad {

// ---------------------------------------------------------------------------
// Data format parsing
// ---------------------------------------------------------------------------

namespace {

// Buffer accessors. Offsets come from the application's own data format, so
// they are validated against dwDataSize at parse time.
LONG ReadAxisAt(const void* buf, int ofs) {
    LONG v;
    std::memcpy(&v, static_cast<const char*>(buf) + ofs, sizeof(v));
    return v;
}

void WriteAxisAt(void* buf, int ofs, LONG v) {
    std::memcpy(static_cast<char*>(buf) + ofs, &v, sizeof(v));
}

DWORD ReadPovAt(const void* buf, int ofs) {
    DWORD v;
    std::memcpy(&v, static_cast<const char*>(buf) + ofs, sizeof(v));
    return v;
}

const char* AxisName(int i) {
    static const char* kNames[kNumAxes] = {"x", "y", "z", "rx", "ry", "rz",
                                           "slider0", "slider1"};
    return (i >= 0 && i < kNumAxes) ? kNames[i] : "?";
}

} // namespace

FormatMap ParseDataFormat(const DIDATAFORMAT* lpdf) {
    FormatMap map;
    if (!lpdf || !lpdf->rgodf || lpdf->dwNumObjs == 0 || lpdf->dwDataSize == 0)
        return map;
    if (lpdf->dwObjSize != sizeof(DIOBJECTDATAFORMAT))
        return map;

    map.dataSize = lpdf->dwDataSize;

    const auto fits = [&](DWORD ofs, DWORD size) {
        return ofs + size <= lpdf->dwDataSize;
    };

    int sliderSeen = 0;

    for (DWORD i = 0; i < lpdf->dwNumObjs; ++i) {
        const DIOBJECTDATAFORMAT& od = lpdf->rgodf[i];

        if (od.dwType & DIDFT_AXIS) {
            if (!fits(od.dwOfs, sizeof(LONG))) continue;

            // c_dfDIJoystick2 also declares velocity / acceleration / force
            // entries for the same axis GUIDs. Only positional data maps to
            // what the game treats as stick/trigger values.
            const DWORD aspect = od.dwFlags & DIDOI_ASPECTMASK;
            if (aspect != 0 && aspect != DIDOI_ASPECTPOSITION) continue;

            int slot = -1;
            if (od.pguid) {
                if      (IsEqualGUID(*od.pguid, GUID_XAxis))  slot = static_cast<int>(Axis::X);
                else if (IsEqualGUID(*od.pguid, GUID_YAxis))  slot = static_cast<int>(Axis::Y);
                else if (IsEqualGUID(*od.pguid, GUID_ZAxis))  slot = static_cast<int>(Axis::Z);
                else if (IsEqualGUID(*od.pguid, GUID_RxAxis)) slot = static_cast<int>(Axis::Rx);
                else if (IsEqualGUID(*od.pguid, GUID_RyAxis)) slot = static_cast<int>(Axis::Ry);
                else if (IsEqualGUID(*od.pguid, GUID_RzAxis)) slot = static_cast<int>(Axis::Rz);
                else if (IsEqualGUID(*od.pguid, GUID_Slider)) {
                    slot = static_cast<int>(Axis::Slider0) + std::min(sliderSeen, 1);
                    ++sliderSeen;
                } else {
                    continue; // some exotic axis we do not model
                }
            } else {
                // NULL GUID = "any axis". DirectInput assigns device objects
                // in enumeration order; approximate with the first canonical
                // slot that is still free.
                for (int s = 0; s < kNumAxes; ++s) {
                    if (map.axisOfs[s] < 0) { slot = s; break; }
                }
            }
            if (slot >= 0 && map.axisOfs[slot] < 0) {
                map.axisOfs[slot] = static_cast<int>(od.dwOfs);
            }
        } else if (od.dwType & DIDFT_POV) {
            if (!fits(od.dwOfs, sizeof(DWORD))) continue;
            if (od.pguid && !IsEqualGUID(*od.pguid, GUID_POV)) continue;
            if (map.povCount < kMaxPovs) {
                map.povOfs[map.povCount++] = static_cast<int>(od.dwOfs);
            }
        } else if (od.dwType & DIDFT_BUTTON) {
            if (!fits(od.dwOfs, sizeof(BYTE))) continue;
            // Exclude keyboard formats (GUID_Key). Joystick button entries
            // use GUID_Button or a NULL "any object" GUID.
            if (od.pguid && !IsEqualGUID(*od.pguid, GUID_Button)) continue;
            if (map.buttonOfs.size() < 128) {
                map.buttonOfs.push_back(static_cast<int>(od.dwOfs));
            }
        }
    }

    const bool hasXY = map.axisOfs[static_cast<int>(Axis::X)] >= 0 &&
                       map.axisOfs[static_cast<int>(Axis::Y)] >= 0;
    map.parsed = hasXY || map.povCount > 0 || !map.buttonOfs.empty();
    return map;
}

// ---------------------------------------------------------------------------
// POV → X/Y
// ---------------------------------------------------------------------------

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
                     const FormatMap& map,
                     const Config& cfg,
                     const AxisRange& xRange,
                     const AxisRange& yRange) {
    if (!buffer || !map.parsed || map.povCount == 0) return;

    const int xOfs = map.axisOfs[static_cast<int>(Axis::X)];
    const int yOfs = map.axisOfs[static_cast<int>(Axis::Y)];
    if (xOfs < 0 && yOfs < 0) return;

    const int idx = std::clamp(cfg.povIndex, 0, map.povCount - 1);
    const DWORD pov = ReadPovAt(buffer, map.povOfs[idx]);

    LONG lX = (xOfs >= 0) ? ReadAxisAt(buffer, xOfs) : 0;
    LONG lY = (yOfs >= 0) ? ReadAxisAt(buffer, yOfs) : 0;

    ApplyOn(lX, lY, pov, cfg, xRange, yRange);

    if (xOfs >= 0) WriteAxisAt(buffer, xOfs, lX);
    if (yOfs >= 0) WriteAxisAt(buffer, yOfs, lY);
}

// ---------------------------------------------------------------------------
// Axis → synthetic button (L2/R2 bridge)
// ---------------------------------------------------------------------------

void ApplyAxisToButtons(void* buffer, const FormatMap& map, const Config& cfg) {
    if (!buffer || !map.parsed) return;
    if (cfg.axisButtons.empty() || map.buttonOfs.empty()) return;

    for (const auto& ab : cfg.axisButtons) {
        const int axisOfs = map.axisOfs[static_cast<int>(ab.axis)];
        if (axisOfs < 0) continue;

        const LONG v = ReadAxisAt(buffer, axisOfs);
        const bool active = (ab.sign >= 0)
            ? (v >=  ab.threshold)
            : (v <= -ab.threshold);
        if (!active) continue;

        if (ab.button < 0) continue;
        if (static_cast<std::size_t>(ab.button) >= map.buttonOfs.size()) continue;

        static_cast<BYTE*>(buffer)[map.buttonOfs[ab.button]] = 0x80;
    }
}

// ---------------------------------------------------------------------------
// AxisDebugDumper
// ---------------------------------------------------------------------------

void AxisDebugDumper::Dump(const void* buffer, const FormatMap& map) {
    if (!buffer || !map.parsed) return;

    long axes[kNumAxes] = {};
    for (int i = 0; i < kNumAxes; ++i) {
        axes[i] = (map.axisOfs[i] >= 0) ? ReadAxisAt(buffer, map.axisOfs[i]) : 0;
    }

    const unsigned long pov0 =
        (map.povCount > 0) ? ReadPovAt(buffer, map.povOfs[0]) : 0xFFFFFFFFu;

    unsigned long long maskLo = 0, maskHi = 0;
    char btnList[256] = {};
    int  off = 0;
    for (std::size_t i = 0; i < map.buttonOfs.size(); ++i) {
        const bool pressed =
            (static_cast<const BYTE*>(buffer)[map.buttonOfs[i]] & 0x80) != 0;
        if (!pressed) continue;
        if (i < 64) maskLo |= (1ull << i);
        else        maskHi |= (1ull << (i - 64));
        int w = std::snprintf(btnList + off, sizeof(btnList) - off,
                              "%s%zu", off == 0 ? "" : ",", i);
        if (w <= 0 || off + w >= static_cast<int>(sizeof(btnList))) break;
        off += w;
    }

    bool changed = !m_haveBaseline ||
        pov0 != m_lastPov ||
        maskLo != m_lastButtonMaskLo || maskHi != m_lastButtonMaskHi;
    for (int i = 0; i < kNumAxes && !changed; ++i) {
        changed = (axes[i] != m_lastAxis[i]);
    }
    if (!changed) return;

    if (off == 0) std::snprintf(btnList, sizeof(btnList), "-");

    Log("[axis-dump] %s=%ld %s=%ld %s=%ld | %s=%ld %s=%ld %s=%ld | %s=%ld %s=%ld | POV0=0x%08lX | btns=%s",
        AxisName(0), axes[0], AxisName(1), axes[1], AxisName(2), axes[2],
        AxisName(3), axes[3], AxisName(4), axes[4], AxisName(5), axes[5],
        AxisName(6), axes[6], AxisName(7), axes[7], pov0, btnList);

    std::memcpy(m_lastAxis, axes, sizeof(axes));
    m_lastPov = pov0;
    m_lastButtonMaskLo = maskLo;
    m_lastButtonMaskHi = maskHi;
    m_haveBaseline = true;
}

} // namespace dipad
