// ADB lifecycle: locate adb, poll for a connected device, set up the port forward,
// and launch the phone app (docs/01 §2.2). Thin wrapper over the adb CLI.
//
// Discovery, `adb devices` parsing, and device-state classification are exposed as
// pure static helpers so they can be unit-tested without a real adb or device.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace phone2pad::client {

// One row from `adb devices` (serial + connection state).
struct AdbDevice {
    std::string serial;
    std::string state;  // "device", "unauthorized", "offline", "no permissions", ...
};

// Overall device situation derived from parsed `adb devices` output.
enum class DeviceStatus {
    None,          // no devices listed
    Unauthorized,  // a device is present but not yet authorized for debugging
    Offline,       // a device is present but offline (still connecting)
    Multiple,      // more than one usable device -> ambiguous which to use
    Ready,         // exactly one usable device in the "device" state
};

struct DeviceQuery {
    DeviceStatus status = DeviceStatus::None;
    std::string serial;  // set only when status == Ready
};

// A candidate adb.exe path together with a human label of where it came from.
struct AdbCandidate {
    std::string path;
    std::string source;  // e.g. "PATH", "Android SDK", "platform-tools next to the app"
};

// Injectable environment for adb discovery. Holding these as plain values lets
// adbCandidatePaths() be tested without touching the real environment/filesystem.
// Empty strings mean "not set"; pathDirs are the entries of %PATH%.
struct AdbEnv {
    std::string configuredAdbPath;  // user-selected adb.exe (config.json); full file path
    std::string androidHome;     // ANDROID_HOME
    std::string androidSdkRoot;  // ANDROID_SDK_ROOT
    std::string localAppData;    // %LOCALAPPDATA%
    std::string exeDir;          // directory containing the running client executable
    std::string cwd;             // current working directory
    std::vector<std::string> pathDirs;  // entries of %PATH%
};

class AdbManager {
public:
    AdbManager(std::string package = "com.phone2pad",
               std::string activity = ".BlackPadActivity",
               int port = 38917);

    // True only when a real adb.exe was found on disk.
    bool ready() const { return found_; }
    const std::string& adbPath() const { return adb_; }
    const std::string& adbSource() const { return source_; }

    // Re-run adb discovery against the current environment and update adb_/source_/
    // found_; returns ready(). Lets a front-end recover after the user installs adb
    // (e.g. drops platform-tools next to the exe) without restarting the process.
    // Not thread-safe: call only when no worker is using this manager (the service
    // stops its worker first — see ClientService::restart()).
    bool recheck();

    // Classify the currently connected devices (runs `adb devices`).
    DeviceQuery queryDevices() const;

    // Serial of the first device in the "device" state, if any.
    std::optional<std::string> firstDevice() const;

    // `adb -s <serial> forward tcp:<port> tcp:<port>`
    bool setupForward(const std::string& serial) const;

    // `adb -s <serial> shell am start -n <package>/<activity>`
    bool launchApp(const std::string& serial) const;

    // ---- pure, testable helpers (no filesystem / process access) ----

    // Ordered candidate adb.exe locations for the given environment. Order:
    // configured path (config.json) -> PATH -> Android SDK (LOCALAPPDATA, then
    // ANDROID_HOME/ANDROID_SDK_ROOT) -> app-local platform-tools (cwd) ->
    // executable-relative platform-tools.
    static std::vector<AdbCandidate> adbCandidatePaths(const AdbEnv& env);

    // Parse raw `adb devices` stdout into device rows.
    static std::vector<AdbDevice> parseDevices(const std::string& devicesOutput);

    // Classify parsed rows into an overall situation.
    static DeviceQuery classifyDevices(const std::vector<AdbDevice>& devices);

private:
    std::string capture(const std::string& args) const;  // stdout of `adb <args>`
    int status(const std::string& args) const;           // exit code of `adb <args>`

    // First candidate that exists on disk, if any.
    static std::optional<AdbCandidate> locateAdb(const AdbEnv& env);
    static AdbEnv currentEnv();

    std::string adb_;      // resolved adb.exe path (empty when not found)
    std::string source_;   // human label of where adb_ was found
    bool found_ = false;
    std::string package_;
    std::string activity_;
    int port_;
};

}  // namespace phone2pad::client
