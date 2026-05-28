#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <string>
#include <vector>
#include <windows.h>

namespace dipad {

namespace {

std::wstring GetIniPathNextToModule() {
    HMODULE hMod = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetIniPathNextToModule),
        &hMod);

    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(hMod, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return L"dipad.ini";
    }

    std::wstring path(buf);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        path = path.substr(0, pos + 1) + L"dipad.ini";
    } else {
        path = L"dipad.ini";
    }
    return path;
}

int IniInt(const wchar_t* section, const wchar_t* key, int def, const std::wstring& path) {
    return GetPrivateProfileIntW(section, key, def, path.c_str());
}

std::wstring IniStr(const wchar_t* section, const wchar_t* key, const wchar_t* def,
                    const std::wstring& path) {
    wchar_t buf[256] = {};
    GetPrivateProfileStringW(section, key, def, buf, 256, path.c_str());
    return buf;
}

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return s;
}

PovMode ParsePovMode(const std::wstring& s) {
    auto v = ToLower(s);
    if (v == L"pov_only" || v == L"povonly" || v == L"only") return PovMode::PovOnly;
    if (v == L"stick_priority" || v == L"stickpriority" || v == L"stick")
        return PovMode::StickPriority;
    return PovMode::PovPriority;
}

bool ParseAxisName(const std::wstring& s, Axis& out) {
    auto v = ToLower(s);
    // Accept either "z" / "rx" or "lz" / "lrx" prefixes for convenience.
    if (!v.empty() && v.front() == L'l') v.erase(v.begin());
    if (v == L"x")        { out = Axis::X;        return true; }
    if (v == L"y")        { out = Axis::Y;        return true; }
    if (v == L"z")        { out = Axis::Z;        return true; }
    if (v == L"rx")       { out = Axis::Rx;       return true; }
    if (v == L"ry")       { out = Axis::Ry;       return true; }
    if (v == L"rz")       { out = Axis::Rz;       return true; }
    if (v == L"slider0" || v == L"s0") { out = Axis::Slider0; return true; }
    if (v == L"slider1" || v == L"s1") { out = Axis::Slider1; return true; }
    return false;
}

// Split "z:+:16000:10" into 4 fields. Any leading/trailing whitespace is
// stripped. Returns false if the count of fields doesn't match.
bool SplitColon(const std::wstring& s, std::vector<std::wstring>& out, size_t expected) {
    out.clear();
    std::wstring cur;
    for (wchar_t ch : s) {
        if (ch == L':') {
            out.push_back(cur);
            cur.clear();
        } else if (!iswspace(ch)) {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out.size() == expected;
}

bool ParseAxisButton(const std::wstring& spec, AxisButton& out) {
    std::vector<std::wstring> parts;
    if (!SplitColon(spec, parts, 4)) return false;

    AxisButton ab;
    if (!ParseAxisName(parts[0], ab.axis)) return false;
    if (parts[1].empty()) return false;
    if (parts[1][0] == L'+' || ToLower(parts[1]) == L"pos") ab.sign = +1;
    else if (parts[1][0] == L'-' || ToLower(parts[1]) == L"neg") ab.sign = -1;
    else return false;
    try {
        ab.threshold = std::stol(parts[2]);
        ab.button    = std::stoi(parts[3]);
    } catch (...) {
        return false;
    }
    if (ab.threshold < 0) ab.threshold = -ab.threshold;
    if (ab.button < 0 || ab.button > 127) return false;

    out = ab;
    return true;
}

} // namespace

Config LoadConfig() {
    Config cfg;
    const auto path = GetIniPathNextToModule();

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return cfg;
    }

    cfg.povMode       = ParsePovMode(IniStr(L"General", L"PovMode", L"pov_priority", path));
    cfg.axisValue     = IniInt(L"General", L"AxisValue", 1000, path);
    cfg.stickDeadzone = IniInt(L"General", L"StickDeadzone", 200, path);
    cfg.enableLog     = (IniInt(L"General", L"EnableLog", 0, path) != 0);
    cfg.overrideLX    = (IniInt(L"General", L"OverrideLX", 1, path) != 0);
    cfg.overrideLY    = (IniInt(L"General", L"OverrideLY", 1, path) != 0);
    cfg.povIndex      = IniInt(L"General", L"PovIndex", 0, path);
    cfg.debugAxisDump = (IniInt(L"General", L"DebugAxisDump", 0, path) != 0);

    if (cfg.povIndex < 0) cfg.povIndex = 0;
    if (cfg.povIndex > 3) cfg.povIndex = 3;
    if (cfg.axisValue < 0) cfg.axisValue = -cfg.axisValue;

    // Read up to 16 axis-to-button mappings under [AxisToButton].
    // Keys are AxisButton1, AxisButton2, ... AxisButton16.
    for (int i = 1; i <= 16; ++i) {
        wchar_t keyBuf[32];
        wsprintfW(keyBuf, L"AxisButton%d", i);
        auto spec = IniStr(L"AxisToButton", keyBuf, L"", path);
        if (spec.empty()) continue;
        AxisButton ab;
        if (ParseAxisButton(spec, ab)) {
            cfg.axisButtons.push_back(ab);
        }
    }

    return cfg;
}

} // namespace dipad
