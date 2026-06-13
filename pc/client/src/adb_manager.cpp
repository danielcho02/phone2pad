#include "phone2pad/client/adb_manager.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include "phone2pad/client/adb_config.hpp"
#include "phone2pad/client/process_runner.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
constexpr char kPathSep = ';';
#else
constexpr char kPathSep = ':';
#endif

namespace phone2pad::client {

namespace {

std::optional<std::string> envVar(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0') return std::nullopt;
    return std::string(v);
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> dirs;
    std::string cur;
    std::istringstream ss(path);
    while (std::getline(ss, cur, kPathSep)) {
        const std::string d = trim(cur);
        if (!d.empty()) dirs.push_back(d);
    }
    return dirs;
}

// adb.exe under <root>/platform-tools/adb.exe, as a string.
std::string sdkAdb(const std::string& root) {
    namespace fs = std::filesystem;
    return (fs::path(root) / "platform-tools" / "adb.exe").string();
}

// adb.exe under <base>/platform-tools/adb.exe (app-local drop-in: unzip the official
// platform-tools next to the exe). Matches the layout documented in ADB-SETUP.md.
std::string localToolsAdb(const std::string& base) {
    namespace fs = std::filesystem;
    return (fs::path(base) / "platform-tools" / "adb.exe").string();
}

}  // namespace

AdbManager::AdbManager(std::string package, std::string activity, int port)
    : package_(std::move(package)), activity_(std::move(activity)), port_(port) {
    if (auto found = locateAdb(currentEnv())) {
        adb_ = found->path;
        source_ = found->source;
        found_ = true;
    }
}

bool AdbManager::recheck() {
    adb_.clear();
    source_.clear();
    found_ = false;
    if (auto found = locateAdb(currentEnv())) {
        adb_ = found->path;
        source_ = found->source;
        found_ = true;
    }
    return found_;
}

AdbEnv AdbManager::currentEnv() {
    namespace fs = std::filesystem;
    AdbEnv env;
    if (auto v = adb_config::loadAdbPath()) env.configuredAdbPath = *v;
    if (auto v = envVar("ANDROID_HOME")) env.androidHome = *v;
    if (auto v = envVar("ANDROID_SDK_ROOT")) env.androidSdkRoot = *v;
    if (auto v = envVar("LOCALAPPDATA")) env.localAppData = *v;
    if (auto v = envVar("PATH")) env.pathDirs = splitPath(*v);

    std::error_code ec;
    env.cwd = fs::current_path(ec).string();

#ifdef _WIN32
    std::array<wchar_t, MAX_PATH> buf{};
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size()) {
        env.exeDir = fs::path(std::wstring(buf.data(), n)).parent_path().string();
    }
#endif
    if (env.exeDir.empty()) env.exeDir = env.cwd;
    return env;
}

std::vector<AdbCandidate> AdbManager::adbCandidatePaths(const AdbEnv& env) {
    namespace fs = std::filesystem;
    std::vector<AdbCandidate> out;

    // 0) User-selected adb.exe (tray "platform-tools 폴더 선택" -> config.json). Takes
    //    priority so an explicit choice wins over whatever happens to be on PATH/SDK.
    if (!env.configuredAdbPath.empty()) {
        out.push_back({env.configuredAdbPath, "configured path"});
    }
    // 1) PATH.
    for (const std::string& dir : env.pathDirs) {
        out.push_back({(fs::path(dir) / "adb.exe").string(), "PATH"});
    }
    // 2) Android SDK: %LOCALAPPDATA%\Android\Sdk, then ANDROID_HOME / ANDROID_SDK_ROOT.
    if (!env.localAppData.empty()) {
        out.push_back({sdkAdb((fs::path(env.localAppData) / "Android" / "Sdk").string()),
                       "Android SDK"});
    }
    if (!env.androidHome.empty()) out.push_back({sdkAdb(env.androidHome), "Android SDK"});
    if (!env.androidSdkRoot.empty()) out.push_back({sdkAdb(env.androidSdkRoot), "Android SDK"});

    // 3) App-local platform-tools (relative to the working directory).
    if (!env.cwd.empty()) {
        out.push_back({localToolsAdb(env.cwd), "platform-tools next to the app"});
    }
    // 4) Executable-relative platform-tools.
    if (!env.exeDir.empty() && env.exeDir != env.cwd) {
        out.push_back({localToolsAdb(env.exeDir), "platform-tools next to the app"});
    }
    return out;
}

std::optional<AdbCandidate> AdbManager::locateAdb(const AdbEnv& env) {
    namespace fs = std::filesystem;
    for (const AdbCandidate& c : adbCandidatePaths(env)) {
        std::error_code ec;
        if (fs::exists(c.path, ec) && fs::is_regular_file(c.path, ec)) return c;
    }
    return std::nullopt;
}

// All adb invocation goes through runHidden() so the child is launched with no
// window and never steals foreground focus (critical when the GUI-subsystem tray is
// the caller — see process_runner.hpp). A conservative timeout keeps a stuck adb from
// hanging the ClientService worker thread.
std::string AdbManager::capture(const std::string& args) const {
    return runHidden(adb_, args, /*captureOutput=*/true).output;
}

int AdbManager::status(const std::string& args) const {
    return runHidden(adb_, args, /*captureOutput=*/false).exitCode;
}

std::vector<AdbDevice> AdbManager::parseDevices(const std::string& devicesOutput) {
    std::vector<AdbDevice> devices;
    std::istringstream lines(devicesOutput);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);  // also drops trailing '\r' on CRLF input
        if (line.empty() || line.rfind("List of devices", 0) == 0) continue;
        if (line[0] == '*') continue;  // daemon banner ("* daemon started ...")

        // adb prints "<serial>\t<state>"; state may contain spaces ("no permissions").
        const std::size_t ws = line.find_first_of(" \t");
        if (ws == std::string::npos) continue;
        AdbDevice d;
        d.serial = line.substr(0, ws);
        d.state = trim(line.substr(ws));
        if (!d.serial.empty() && !d.state.empty()) devices.push_back(std::move(d));
    }
    return devices;
}

DeviceQuery AdbManager::classifyDevices(const std::vector<AdbDevice>& devices) {
    DeviceQuery q;
    std::vector<const AdbDevice*> ready;
    bool anyUnauthorized = false;
    bool anyOffline = false;
    for (const AdbDevice& d : devices) {
        if (d.state == "device") {
            ready.push_back(&d);
        } else if (d.state == "unauthorized") {
            anyUnauthorized = true;
        } else if (d.state == "offline") {
            anyOffline = true;
        }
    }
    if (ready.size() == 1) {
        q.status = DeviceStatus::Ready;
        q.serial = ready.front()->serial;
    } else if (ready.size() > 1) {
        q.status = DeviceStatus::Multiple;
    } else if (anyUnauthorized) {
        q.status = DeviceStatus::Unauthorized;
    } else if (anyOffline) {
        q.status = DeviceStatus::Offline;
    } else {
        q.status = DeviceStatus::None;
    }
    return q;
}

DeviceQuery AdbManager::queryDevices() const {
    return classifyDevices(parseDevices(capture("devices")));
}

std::optional<std::string> AdbManager::firstDevice() const {
    const DeviceQuery q = queryDevices();
    if (q.status == DeviceStatus::Ready) return q.serial;
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
