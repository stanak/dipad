#pragma once

#include "config.h"

#include <cstddef>

namespace dipad {

// Apply POV → lX/lY remapping on a buffer that is either a DIJOYSTATE or a
// DIJOYSTATE2 (detected by cbData). Other buffer sizes are ignored.
//
// The remap honors:
//   - cfg.povMode
//   - cfg.axisValue
//   - cfg.stickDeadzone
//   - cfg.povIndex
//   - cfg.overrideLX / cfg.overrideLY
//
// Range information (DIPROP_RANGE on axes) is captured per-device and passed
// here through axisMin / axisMax so the output magnitude matches whatever the
// game asked for, when the game customized it.
struct AxisRange {
    long min = -1000;
    long max = +1000;
    bool set = false;
};

void ApplyPovToStick(void* buffer,
                     std::size_t cbData,
                     const Config& cfg,
                     const AxisRange& xRange,
                     const AxisRange& yRange);

// Apply axis → synthetic button presses for L2/R2 style triggers. For every
// entry in cfg.axisButtons, reads the chosen axis from the state buffer and,
// if its value crosses the threshold in the requested direction, OR-s a
// 0x80 into rgbButtons[ab.button]. The original axis value is left intact
// so games that DO read the axis still see it.
void ApplyAxisToButtons(void* buffer,
                        std::size_t cbData,
                        const Config& cfg);

// Optional debug helper: writes the full state (all axes, POV, pressed
// buttons) into dipad.log when fields change. Only active when cfg.enableLog
// AND cfg.debugAxisDump are both true. Stateful — keep one instance per
// device wrapper.
class AxisDebugDumper {
public:
    void Dump(const void* buffer, std::size_t cbData);
private:
    long  m_lastX = 0, m_lastY = 0, m_lastZ = 0;
    long  m_lastRx = 0, m_lastRy = 0, m_lastRz = 0;
    long  m_lastSlider0 = 0, m_lastSlider1 = 0;
    unsigned long m_lastPov = 0xFFFFFFFFu;
    unsigned long long m_lastButtonMaskLo = 0;
    unsigned long long m_lastButtonMaskHi = 0;
    bool  m_haveBaseline = false;
};

} // namespace dipad
