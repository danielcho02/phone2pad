// Unit tests for the pure autostart helpers: Run-key command formatting (always
// quoted) and stale-entry match detection. No real registry access — these exercise
// the string logic that setEnabled()/isEnabled() build on.
#include "phone2pad/client/autostart.hpp"

#include <string>

#include "test_main.hpp"

using namespace phone2pad::client;

TEST_CASE("autostart runCommand always double-quotes the exe path") {
    const std::wstring path = L"C:\\Program Files\\phone2pad\\phone2pad_tray.exe";
    CHECK(autostart::runCommand(path) ==
          L"\"C:\\Program Files\\phone2pad\\phone2pad_tray.exe\"");
}

TEST_CASE("autostart commandMatches: quoted stored value matches bare path") {
    const std::wstring path = L"C:\\apps\\phone2pad\\phone2pad_tray.exe";
    CHECK(autostart::commandMatches(autostart::runCommand(path), path));
    CHECK(autostart::commandMatches(path, path));  // unquoted both sides
}

TEST_CASE("autostart commandMatches is case-insensitive (Windows paths)") {
    CHECK(autostart::commandMatches(L"\"C:\\Apps\\Phone2Pad\\phone2pad_tray.exe\"",
                                    L"c:\\apps\\phone2pad\\PHONE2PAD_TRAY.EXE"));
}

TEST_CASE("autostart commandMatches: different path does not match") {
    CHECK(!autostart::commandMatches(L"\"C:\\old\\phone2pad_tray.exe\"",
                                     L"C:\\new\\phone2pad_tray.exe"));
}
