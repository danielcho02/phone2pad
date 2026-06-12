// Hidden child-process runner (v0.3.0 tray hotfix). The tray runs as a GUI-subsystem
// process with no console, so launching adb via _popen/std::system (which go through
// cmd.exe) allocates a console window on every poll — that flashes a window and
// steals keyboard focus. runHidden() launches the child with CreateProcessW +
// CREATE_NO_WINDOW + a hidden STARTUPINFO so NO window is ever created and nothing is
// brought to the foreground, capturing stdout/stderr through an anonymous pipe.
//
// A conservative timeout (default 10s) guards the tray's worker thread: a stuck adb
// child is terminated rather than hanging the service forever. The UI thread never
// blocks on adb — only ClientService's worker thread calls into AdbManager.
#pragma once

#include <string>

namespace phone2pad::client {

struct ProcessResult {
    bool launched = false;  // CreateProcess (or popen) succeeded
    int exitCode = -1;      // child exit code; -1 if not launched or timed out/killed
    std::string output;     // captured stdout+stderr when captureOutput is set
};

// Run `exePath` with the argument string `args`, fully hidden and non-foreground.
// captureOutput=true reads the child's stdout+stderr via a pipe (no visible console).
// timeoutMs caps the wait; on timeout the child is terminated and exitCode is -1.
ProcessResult runHidden(const std::string& exePath, const std::string& args,
                        bool captureOutput, int timeoutMs = 10000);

#ifdef _WIN32
// ---- testable seams (Windows) ----

// Full command line for CreateProcessW: the exe path quoted as a single token,
// followed by the raw argument string. Pure — no process/filesystem access.
std::wstring buildProcessCommandLine(const std::string& exePath, const std::string& args);

// The exact creation/show flags runHidden() relies on to stay invisible. Exposed so a
// unit test can pin them and they cannot silently regress. Fields are plain integers
// (matching the Win32 macro values) so this header needs no <windows.h>.
struct HiddenSpawnSpec {
    unsigned long creationFlags;   // == CREATE_NO_WINDOW
    unsigned short showWindow;     // == SW_HIDE
    unsigned long startupFlags;    // STARTF_USESHOWWINDOW (+ STARTF_USESTDHANDLES if capturing)
};
HiddenSpawnSpec hiddenSpawnSpec(bool captureOutput);
#endif

}  // namespace phone2pad::client
