#pragma once

#include <string>
#include <windows.h>

namespace dipad {

// Initialize the optional log file at %LOCALAPPDATA%\dipad\dipad.log. We use
// LOCALAPPDATA rather than the game folder so we don't need write access to
// Program Files. If logging is disabled in the config, this is a no-op.
void InitLogger(bool enabled);
void ShutdownLogger();

void Log(const char* fmt, ...);

// Returns the canonical dipad log directory (%LOCALAPPDATA%\dipad\). Used by
// DllMain to drop the load marker next to other logs.
std::wstring GetLogDirectory();

} // namespace dipad
