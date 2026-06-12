// Tests for the hidden process runner. The pure seams (command-line quoting, the
// hidden-spawn flag set) pin the no-window contract so it cannot regress, and a couple
// of small smoke runs exercise capture + the timeout/terminate path. Windows-only:
// the runner's no-console behavior is a Win32 concern.
#include "phone2pad/client/process_runner.hpp"

#include <string>

#include "test_main.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace phone2pad::client;

TEST_CASE("buildProcessCommandLine quotes the exe and appends args") {
    CHECK(buildProcessCommandLine("C:\\Program Files\\platform-tools\\adb.exe",
                                  "-s ABC devices") ==
          L"\"C:\\Program Files\\platform-tools\\adb.exe\" -s ABC devices");
}

TEST_CASE("buildProcessCommandLine with no args is just the quoted exe") {
    CHECK(buildProcessCommandLine("adb.exe", "") == L"\"adb.exe\"");
}

TEST_CASE("hiddenSpawnSpec pins the no-window flags (regression guard)") {
    const HiddenSpawnSpec noCap = hiddenSpawnSpec(false);
    CHECK(noCap.creationFlags == CREATE_NO_WINDOW);
    CHECK(noCap.showWindow == SW_HIDE);
    CHECK((noCap.startupFlags & STARTF_USESHOWWINDOW) != 0);
    CHECK((noCap.startupFlags & STARTF_USESTDHANDLES) == 0);  // no pipe when not capturing

    const HiddenSpawnSpec cap = hiddenSpawnSpec(true);
    CHECK(cap.creationFlags == CREATE_NO_WINDOW);
    CHECK(cap.showWindow == SW_HIDE);
    CHECK((cap.startupFlags & STARTF_USESHOWWINDOW) != 0);
    CHECK((cap.startupFlags & STARTF_USESTDHANDLES) != 0);  // pipe stdout/stderr
}

TEST_CASE("runHidden captures output from a quick command, no window") {
    // whoami.exe is always present on Windows; resolved via PATH by CreateProcessW.
    const ProcessResult r = runHidden("whoami", "", /*captureOutput=*/true);
    CHECK(r.launched);
    CHECK(r.exitCode == 0);
    CHECK(!r.output.empty());
}

TEST_CASE("runHidden terminates a child that exceeds the timeout") {
    // ping -n 5 runs ~4s; a 200ms timeout must terminate it and report exitCode -1.
    const ProcessResult r =
        runHidden("ping", "127.0.0.1 -n 5", /*captureOutput=*/false, /*timeoutMs=*/200);
    CHECK(r.launched);
    CHECK(r.exitCode == -1);
}

#endif  // _WIN32
