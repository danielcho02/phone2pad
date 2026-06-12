// Autostart-on-login for phone2pad_tray.exe via the per-user HKCU Run key
// (Software\Microsoft\Windows\CurrentVersion\Run). Per-user means no admin rights
// and trivial enable/disable, which suits a lightweight tray companion.
// https://learn.microsoft.com/windows/win32/setupapi/run-and-runonce-registry-keys
//
// The string logic (quoting, match-checking) is split out as pure functions so it is
// unit-testable without touching the real registry.
#pragma once

#include <string>

namespace phone2pad::client::autostart {

// Value name written under the HKCU Run key.
inline constexpr wchar_t kValueName[] = L"phone2pad";
// Sub-key path under HKEY_CURRENT_USER.
inline constexpr wchar_t kRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// ---- pure helpers (no registry / filesystem access) ----

// The Run-key command for an exe path: always double-quoted so a path containing
// spaces (e.g. C:\Program Files\...) is parsed as a single argument.
std::wstring runCommand(const std::wstring& exePath);

// True iff a stored Run value points at exePath, ignoring surrounding quotes and
// ASCII case (Windows paths are case-insensitive). Used to detect a stale entry.
bool commandMatches(const std::wstring& storedValue, const std::wstring& exePath);

#ifdef _WIN32
// ---- registry-backed operations (Windows only) ----

// Full path of the running executable (GetModuleFileNameW). Empty on failure.
std::wstring currentExePath();

// True iff the HKCU Run value exists (autostart enabled). Quote/path-agnostic.
bool isEnabled();

// Enable -> write the Run value pointing at the current exe (re-points if stale).
// Disable -> delete the value. Returns true on success.
bool setEnabled(bool enable);
#endif

}  // namespace phone2pad::client::autostart
