#include "phone2pad/client/autostart.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace phone2pad::client::autostart {

namespace {

// Drop one layer of surrounding double quotes, if present.
std::wstring unquote(const std::wstring& s) {
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

// ASCII-case-insensitive equality (sufficient for Windows file paths).
bool iequalsAscii(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        wchar_t ca = a[i], cb = b[i];
        if (ca >= L'A' && ca <= L'Z') ca = static_cast<wchar_t>(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z') cb = static_cast<wchar_t>(cb - L'A' + L'a');
        if (ca != cb) return false;
    }
    return true;
}

}  // namespace

std::wstring runCommand(const std::wstring& exePath) {
    return L"\"" + exePath + L"\"";
}

bool commandMatches(const std::wstring& storedValue, const std::wstring& exePath) {
    return iequalsAscii(unquote(storedValue), unquote(exePath));
}

#ifdef _WIN32

std::wstring currentExePath() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();  // truncated/long-path failure
    return std::wstring(buf, n);
}

bool isEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    const LONG r = RegQueryValueExW(key, kValueName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool setEnabled(bool enable) {
    HKEY key;
    // The Run key exists on every install, but create-or-open is harmless and robust.
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG r;
    if (enable) {
        const std::wstring cmd = runCommand(currentExePath());
        // +1 to include the terminating NUL in the byte count (REG_SZ convention).
        const DWORD bytes = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
        r = RegSetValueExW(key, kValueName, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(cmd.c_str()), bytes);
    } else {
        r = RegDeleteValueW(key, kValueName);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;  // already absent == success
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

#endif  // _WIN32

}  // namespace phone2pad::client::autostart
