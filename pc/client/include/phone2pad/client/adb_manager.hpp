// ADB lifecycle: locate adb, poll for a connected device, set up the port forward,
// and launch the phone app (docs/01 §2.2). Thin wrapper over the adb CLI.
#pragma once

#include <optional>
#include <string>

namespace phone2pad::client {

class AdbManager {
public:
    AdbManager(std::string package = "com.phone2pad",
               std::string activity = ".BlackPadActivity",
               int port = 38917);

    bool ready() const { return !adb_.empty(); }
    const std::string& adbPath() const { return adb_; }

    // Serial of the first device in the "device" state, if any.
    std::optional<std::string> firstDevice() const;

    // `adb -s <serial> forward tcp:<port> tcp:<port>`
    bool setupForward(const std::string& serial) const;

    // `adb -s <serial> shell am start -n <package>/<activity>`
    bool launchApp(const std::string& serial) const;

private:
    std::string capture(const std::string& args) const;  // stdout of `adb <args>`
    int status(const std::string& args) const;           // exit code of `adb <args>`
    static std::string locateAdb();

    std::string adb_;
    std::string package_;
    std::string activity_;
    int port_;
};

}  // namespace phone2pad::client
