#pragma once

#include <string>
#include <vector>

namespace dipad {

enum class PovMode {
    PovPriority,  // POV overrides stick only when POV is being pressed (default)
    PovOnly,      // POV always determines lX/lY (stick ignored on X/Y)
    StickPriority // POV is used only when stick is centered
};

// DInput axes that can be mapped to synthetic button presses. The names
// correspond to fields in DIJOYSTATE / DIJOYSTATE2 (lX, lY, lZ, lRx, lRy,
// lRz, rglSlider[0], rglSlider[1]).
enum class Axis {
    X,
    Y,
    Z,
    Rx,
    Ry,
    Rz,
    Slider0,
    Slider1,
};

// Synthesize a button press when the chosen axis is on the chosen side of
// the threshold. Used to expose L2/R2-style analog triggers to games that
// only know how to bind buttons (e.g. 非想天則 ignores axis input in its
// key config).
struct AxisButton {
    Axis axis     = Axis::Z;
    int  sign     = +1;     // +1 = trigger when value >= +threshold
                            // -1 = trigger when value <= -threshold
    long threshold = 16000; // absolute magnitude relative to the device's
                            // current range (use DebugAxisDump=1 to inspect
                            // real-time values from your controller).
    int  button   = 0;      // 0..127 (DIJOYSTATE2 has 128 buttons,
                            // DIJOYSTATE has 32).
};

struct Config {
    PovMode  povMode      = PovMode::PovPriority;
    long     axisValue    = 1000;   // Magnitude written to lX / lY for POV directions.
    int      stickDeadzone = 200;   // Considered "centered" if |stick| <= this (in default DI range).
    bool     enableLog     = false; // If true, write dipad.log into %LOCALAPPDATA%\dipad\.
    bool     overrideLX    = true;
    bool     overrideLY    = true;
    int      povIndex      = 0;     // Which POV (0..3) to use as direction source.

    // L2/R2 → button bridge. Populated from the [AxisToButton] ini section.
    std::vector<AxisButton> axisButtons;

    // If true, periodically dump axis & button state into dipad.log so users
    // can identify which axis their controller's L2/R2 lives on, and which
    // button slots are free for synthesis.
    bool     debugAxisDump = false;
};

// Load the config from dipad.ini located next to the shim DLL.
// Missing file / missing keys silently use defaults.
Config LoadConfig();

} // namespace dipad
