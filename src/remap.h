#pragma once

#include "config.h"

#include <cstddef>
#include <vector>

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dipad {

constexpr int kNumAxes = 8; // matches enum class Axis (X..Slider1)
constexpr int kMaxPovs = 4;

// Describes where the joystick objects live inside the application's data
// format buffer. Built from the DIDATAFORMAT passed to SetDataFormat, so the
// remap works with c_dfDIJoystick / c_dfDIJoystick2 AND with custom formats
// (offsets are read from the format instead of assuming the DIJOYSTATE
// layout). An offset of -1 means "the format does not contain this object".
struct FormatMap {
    int axisOfs[kNumAxes] = {-1, -1, -1, -1, -1, -1, -1, -1}; // indexed by Axis
    int povOfs[kMaxPovs]  = {-1, -1, -1, -1};
    int povCount          = 0;

    // buttonOfs[n] = buffer offset of the button the application calls
    // "button n" (declaration order in the format, which matches rgbButtons
    // indexing for the standard joystick formats).
    std::vector<int> buttonOfs;

    unsigned long dataSize = 0;
    bool parsed            = false;
};

// Parse the application's data format into a FormatMap. Returns a map with
// parsed == false when the format carries no joystick-like objects (e.g.
// keyboard / mouse formats) or is malformed.
FormatMap ParseDataFormat(const DIDATAFORMAT* lpdf);

// Range information (DIPROP_RANGE on axes) is captured per-device so the POV
// synthesized values match whatever range the game asked for.
struct AxisRange {
    long min = -1000;
    long max = +1000;
    bool set = false;
};

// Apply POV → X/Y axis remapping in-place on a GetDeviceState buffer laid out
// according to `map`. Honors cfg.povMode / axisValue / stickDeadzone /
// povIndex / overrideLX / overrideLY.
void ApplyPovToStick(void* buffer,
                     const FormatMap& map,
                     const Config& cfg,
                     const AxisRange& xRange,
                     const AxisRange& yRange);

// Apply axis → synthetic button presses for L2/R2 style triggers. For every
// entry in cfg.axisButtons, reads the chosen axis from the state buffer and,
// if its value crosses the threshold in the requested direction, OR-s 0x80
// into the application's button slot. The original axis value is left intact
// so games that DO read the axis still see it.
void ApplyAxisToButtons(void* buffer,
                        const FormatMap& map,
                        const Config& cfg);

// Optional debug helper: writes the full state (all axes, POV, pressed
// buttons) into dipad.log when fields change. Only active when cfg.enableLog
// AND cfg.debugAxisDump are both true. Stateful — keep one instance per
// device wrapper.
class AxisDebugDumper {
public:
    void Dump(const void* buffer, const FormatMap& map);
private:
    long m_lastAxis[kNumAxes] = {};
    unsigned long m_lastPov = 0xFFFFFFFFu;
    unsigned long long m_lastButtonMaskLo = 0;
    unsigned long long m_lastButtonMaskHi = 0;
    bool m_haveBaseline = false;
};

} // namespace dipad
