#include "phone2pad/client/process_runner.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <thread>
#include <vector>

namespace phone2pad::client {

namespace {

std::wstring widenUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                      nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

}  // namespace

std::wstring buildProcessCommandLine(const std::string& exePath, const std::string& args) {
    std::wstring cmd = L"\"" + widenUtf8(exePath) + L"\"";
    if (!args.empty()) {
        cmd += L" ";
        cmd += widenUtf8(args);
    }
    return cmd;
}

HiddenSpawnSpec hiddenSpawnSpec(bool captureOutput) {
    HiddenSpawnSpec s{};
    // CREATE_NO_WINDOW is the flag that prevents a console from being allocated for a
    // console child launched from a windowless (GUI-subsystem) process — the actual
    // fix for the flashing-window / focus-stealing bug.
    s.creationFlags = CREATE_NO_WINDOW;
    s.showWindow = SW_HIDE;
    s.startupFlags = STARTF_USESHOWWINDOW;
    if (captureOutput) s.startupFlags |= STARTF_USESTDHANDLES;
    return s;
}

ProcessResult runHidden(const std::string& exePath, const std::string& args,
                        bool captureOutput, int timeoutMs) {
    ProcessResult result;

    const HiddenSpawnSpec spec = hiddenSpawnSpec(captureOutput);
    std::wstring cmdLine = buildProcessCommandLine(exePath, args);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = spec.startupFlags;
    si.wShowWindow = spec.showWindow;

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (captureOutput) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;  // child must inherit the write end
        if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) {
            return result;  // launched=false
        }
        // The read end stays with the parent only — never inherited by the child.
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = writeEnd;
        si.hStdError = writeEnd;
        si.hStdInput = nullptr;  // adb polling needs no stdin
    }

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        nullptr, cmdLine.data(), nullptr, nullptr,
        /*bInheritHandles=*/captureOutput ? TRUE : FALSE, spec.creationFlags, nullptr,
        nullptr, &si, &pi);

    // The parent never writes to the pipe; close its write end so ReadFile sees EOF
    // when the child exits (or is terminated).
    if (writeEnd != nullptr) CloseHandle(writeEnd);

    if (!ok) {
        if (readEnd != nullptr) CloseHandle(readEnd);
        return result;  // launched=false, exitCode=-1
    }
    result.launched = true;

    // Drain stdout on a dedicated thread so a large/blocking child output cannot fill
    // the pipe and deadlock against our timed wait below.
    std::thread reader;
    std::string captured;
    if (captureOutput) {
        reader = std::thread([readEnd, &captured]() {
            std::array<char, 1024> buf{};
            DWORD n = 0;
            while (ReadFile(readEnd, buf.data(), static_cast<DWORD>(buf.size()), &n,
                            nullptr) &&
                   n > 0) {
                captured.append(buf.data(), n);
            }
        });
    }

    const DWORD waited =
        WaitForSingleObject(pi.hProcess, timeoutMs > 0 ? static_cast<DWORD>(timeoutMs)
                                                       : INFINITE);
    if (waited == WAIT_TIMEOUT) {
        // Stuck child: terminate it so the worker thread is never hung. This closes the
        // child's pipe handle, so the reader thread then hits EOF and finishes.
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        result.exitCode = -1;
    } else {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) result.exitCode = static_cast<int>(code);
    }

    if (reader.joinable()) reader.join();
    if (readEnd != nullptr) CloseHandle(readEnd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    result.output = std::move(captured);
    return result;
}

}  // namespace phone2pad::client

#else  // ----- non-Windows fallback (project is Windows-only in practice) -----

#include <array>
#include <cstdio>
#include <cstdlib>

namespace phone2pad::client {

ProcessResult runHidden(const std::string& exePath, const std::string& args,
                        bool captureOutput, int /*timeoutMs*/) {
    ProcessResult result;
    std::string cmd = "\"" + exePath + "\" " + args;
    if (captureOutput) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe == nullptr) return result;
        result.launched = true;
        std::array<char, 512> buf{};
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
            result.output += buf.data();
        }
        result.exitCode = pclose(pipe);
    } else {
        result.launched = true;
        result.exitCode = std::system(cmd.c_str());
    }
    return result;
}

}  // namespace phone2pad::client

#endif  // _WIN32
