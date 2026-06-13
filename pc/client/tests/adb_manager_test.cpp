// Unit tests for AdbManager's pure helpers: adb.exe discovery ordering,
// `adb devices` output parsing, and device-state classification. None of these
// touch a real adb or device.
#include "phone2pad/client/adb_manager.hpp"

#include <string>
#include <vector>

#include "test_main.hpp"

using namespace phone2pad::client;

namespace {

// Count candidates whose source matches `source`.
std::size_t countSource(const std::vector<AdbCandidate>& c, const std::string& source) {
    std::size_t n = 0;
    for (const auto& e : c) {
        if (e.source == source) ++n;
    }
    return n;
}

// First index of a candidate with the given source (or npos-like sentinel).
std::size_t indexOfSource(const std::vector<AdbCandidate>& c, const std::string& source) {
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c[i].source == source) return i;
    }
    return c.size();
}

}  // namespace

TEST_CASE("adb candidate ordering: PATH, SDK, app-local, exe-relative") {
    AdbEnv env;
    env.pathDirs = {"C:\\path-a", "C:\\path-b"};
    env.localAppData = "C:\\Users\\u\\AppData\\Local";
    env.cwd = "C:\\app";
    env.exeDir = "C:\\install";

    const auto c = AdbManager::adbCandidatePaths(env);

    // Every documented source is represented.
    CHECK(countSource(c, "PATH") == 2);
    CHECK(countSource(c, "Android SDK") >= 1);
    CHECK(countSource(c, "platform-tools next to the app") == 2);

    // Documented order: PATH < Android SDK < app-local/exe-relative platform-tools.
    const std::size_t iPath = indexOfSource(c, "PATH");
    const std::size_t iSdk = indexOfSource(c, "Android SDK");
    const std::size_t iTools = indexOfSource(c, "platform-tools next to the app");
    CHECK(iPath < iSdk);
    CHECK(iSdk < iTools);

    // PATH entries become "<dir>\adb.exe" in PATH order.
    CHECK(c[0].path.find("path-a") != std::string::npos);
    CHECK(c[0].path.find("adb.exe") != std::string::npos);
    CHECK(c[1].path.find("path-b") != std::string::npos);

    // App-local drop-in is "<dir>\platform-tools\adb.exe" (flat next to the exe),
    // NOT the old "<dir>\tools\platform-tools\..." layout — guard against regressing
    // the documented location.
    CHECK(c[iTools].path.find("platform-tools\\adb.exe") != std::string::npos);
    CHECK(c[iTools].path.find("tools\\platform-tools") == std::string::npos);
}

TEST_CASE("adb candidate: configured path is first, before PATH") {
    AdbEnv env;
    env.configuredAdbPath = "C:\\Users\\u\\Downloads\\platform-tools\\adb.exe";
    env.pathDirs = {"C:\\path-a"};
    env.localAppData = "C:\\Users\\u\\AppData\\Local";

    const auto c = AdbManager::adbCandidatePaths(env);
    CHECK(!c.empty());
    CHECK_EQ(c[0].source, std::string("configured path"));
    CHECK_EQ(c[0].path, env.configuredAdbPath);

    // It must precede PATH (an explicit choice wins).
    const std::size_t iConfigured = indexOfSource(c, "configured path");
    const std::size_t iPath = indexOfSource(c, "PATH");
    CHECK(iConfigured < iPath);
}

TEST_CASE("adb candidate: no configured path adds no configured candidate") {
    AdbEnv env;
    env.pathDirs = {"C:\\path-a"};
    const auto c = AdbManager::adbCandidatePaths(env);
    CHECK(countSource(c, "configured path") == 0);
}

TEST_CASE("adb candidate: LOCALAPPDATA SDK path shape") {
    AdbEnv env;
    env.localAppData = "C:\\Users\\u\\AppData\\Local";
    const auto c = AdbManager::adbCandidatePaths(env);
    const std::size_t i = indexOfSource(c, "Android SDK");
    CHECK(i < c.size());
    CHECK(c[i].path.find("Android") != std::string::npos);
    CHECK(c[i].path.find("Sdk") != std::string::npos);
    CHECK(c[i].path.find("platform-tools") != std::string::npos);
}

TEST_CASE("adb candidate: empty environment yields no candidates") {
    AdbEnv env;  // all empty
    const auto c = AdbManager::adbCandidatePaths(env);
    CHECK(c.empty());
}

TEST_CASE("adb candidate: cwd == exeDir does not duplicate app-local entry") {
    AdbEnv env;
    env.cwd = "C:\\same";
    env.exeDir = "C:\\same";
    const auto c = AdbManager::adbCandidatePaths(env);
    CHECK(countSource(c, "platform-tools next to the app") == 1);
}

TEST_CASE("parseDevices: empty / header-only") {
    CHECK(AdbManager::parseDevices("").empty());
    CHECK(AdbManager::parseDevices("List of devices attached\n").empty());
    CHECK(AdbManager::parseDevices("List of devices attached\n\n").empty());
}

TEST_CASE("parseDevices: single ready device") {
    const auto d = AdbManager::parseDevices("List of devices attached\nABC123\tdevice\n");
    CHECK_EQ(d.size(), static_cast<std::size_t>(1));
    CHECK_EQ(d[0].serial, std::string("ABC123"));
    CHECK_EQ(d[0].state, std::string("device"));
}

TEST_CASE("parseDevices: unauthorized and offline states") {
    const auto u = AdbManager::parseDevices("List of devices attached\nXYZ\tunauthorized\n");
    CHECK_EQ(u.size(), static_cast<std::size_t>(1));
    CHECK_EQ(u[0].state, std::string("unauthorized"));

    const auto o = AdbManager::parseDevices("List of devices attached\nXYZ\toffline\n");
    CHECK_EQ(o.size(), static_cast<std::size_t>(1));
    CHECK_EQ(o[0].state, std::string("offline"));
}

TEST_CASE("parseDevices: multi-word state is captured whole") {
    const auto d = AdbManager::parseDevices("List of devices attached\nUSB\tno permissions\n");
    CHECK_EQ(d.size(), static_cast<std::size_t>(1));
    CHECK_EQ(d[0].serial, std::string("USB"));
    CHECK_EQ(d[0].state, std::string("no permissions"));
}

TEST_CASE("parseDevices: CRLF and daemon banner") {
    const std::string out =
        "* daemon not running; starting now at tcp:5037\r\n"
        "* daemon started successfully\r\n"
        "List of devices attached\r\n"
        "ABC\tdevice\r\n";
    const auto d = AdbManager::parseDevices(out);
    CHECK_EQ(d.size(), static_cast<std::size_t>(1));
    CHECK_EQ(d[0].serial, std::string("ABC"));
    CHECK_EQ(d[0].state, std::string("device"));
}

TEST_CASE("parseDevices: multiple devices") {
    const auto d = AdbManager::parseDevices(
        "List of devices attached\nA\tdevice\nB\tdevice\nC\toffline\n");
    CHECK_EQ(d.size(), static_cast<std::size_t>(3));
}

TEST_CASE("classifyDevices: none") {
    const auto q = AdbManager::classifyDevices({});
    CHECK(q.status == DeviceStatus::None);
}

TEST_CASE("classifyDevices: ready returns serial") {
    const auto q = AdbManager::classifyDevices({{"ABC", "device"}});
    CHECK(q.status == DeviceStatus::Ready);
    CHECK_EQ(q.serial, std::string("ABC"));
}

TEST_CASE("classifyDevices: unauthorized") {
    const auto q = AdbManager::classifyDevices({{"ABC", "unauthorized"}});
    CHECK(q.status == DeviceStatus::Unauthorized);
}

TEST_CASE("classifyDevices: offline") {
    const auto q = AdbManager::classifyDevices({{"ABC", "offline"}});
    CHECK(q.status == DeviceStatus::Offline);
}

TEST_CASE("classifyDevices: multiple ready devices") {
    const auto q = AdbManager::classifyDevices({{"A", "device"}, {"B", "device"}});
    CHECK(q.status == DeviceStatus::Multiple);
    CHECK(q.serial.empty());
}

TEST_CASE("classifyDevices: one ready among unauthorized still ready") {
    const auto q = AdbManager::classifyDevices({{"A", "device"}, {"B", "unauthorized"}});
    CHECK(q.status == DeviceStatus::Ready);
    CHECK_EQ(q.serial, std::string("A"));
}

TEST_CASE("classifyDevices: unauthorized takes priority over offline when none ready") {
    const auto q = AdbManager::classifyDevices({{"A", "offline"}, {"B", "unauthorized"}});
    CHECK(q.status == DeviceStatus::Unauthorized);
}
