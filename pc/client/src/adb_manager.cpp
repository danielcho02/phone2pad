#include "phone2pad/client/adb_manager.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#define PP_POPEN _popen
#define PP_PCLOSE _pclose
#else
#define PP_POPEN popen
#define PP_PCLOSE pclose
#endif

namespace phone2pad::client {

namespace {

std::optional<std::string> envVar(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0') return std::nullopt;
    return std::string(v);
}

}  // namespace

AdbManager::AdbManager(std::string package, std::string activity, int port)
    : adb_(locateAdb()),
      package_(std::move(package)),
      activity_(std::move(activity)),
      port_(port) {}

std::string AdbManager::locateAdb() {
    namespace fs = std::filesystem;
    for (const char* envName : {"ANDROID_HOME", "ANDROID_SDK_ROOT"}) {
        if (auto root = envVar(envName)) {
            fs::path p = fs::path(*root) / "platform-tools" / "adb.exe";
            std::error_code ec;
            if (fs::exists(p, ec)) return p.string();
        }
    }
    return "adb";  // fall back to PATH
}

std::string AdbManager::capture(const std::string& args) const {
    std::ostringstream cmd;
    cmd << '"' << adb_ << "\" " << args;
    FILE* pipe = PP_POPEN(cmd.str().c_str(), "r");
    if (pipe == nullptr) return {};
    std::string out;
    std::array<char, 512> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        out += buf.data();
    }
    PP_PCLOSE(pipe);
    return out;
}

int AdbManager::status(const std::string& args) const {
    std::ostringstream cmd;
    cmd << '"' << adb_ << "\" " << args;
    return std::system(cmd.str().c_str());
}

std::optional<std::string> AdbManager::firstDevice() const {
    const std::string out = capture("devices");
    std::istringstream lines(out);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line.rfind("List of devices", 0) == 0) continue;
        std::istringstream cols(line);
        std::string serial, state;
        cols >> serial >> state;
        if (!serial.empty() && state == "device") return serial;
    }
    return std::nullopt;
}

bool AdbManager::setupForward(const std::string& serial) const {
    std::ostringstream args;
    args << "-s " << serial << " forward tcp:" << port_ << " tcp:" << port_;
    return status(args.str()) == 0;
}

bool AdbManager::launchApp(const std::string& serial) const {
    std::ostringstream args;
    args << "-s " << serial << " shell am start -n " << package_ << '/' << activity_;
    return status(args.str()) == 0;
}

}  // namespace phone2pad::client
