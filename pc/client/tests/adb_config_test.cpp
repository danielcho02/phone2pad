// Unit tests for adb_config's pure (de)serialization. The filesystem wrappers
// (load/save/configFilePath/adbExeUnder) touch real paths and are exercised manually
// (L4); these cover the JSON round-trip that the config format depends on.
#include "phone2pad/client/adb_config.hpp"

#include <string>

#include "test_main.hpp"

using namespace phone2pad::client;

TEST_CASE("adb_config: serialize/parse round-trips a Windows path") {
    const std::string path = "C:\\Users\\u\\Downloads\\platform-tools\\adb.exe";
    const std::string json = adb_config::serialize(path);
    const auto parsed = adb_config::parseAdbPath(json);
    CHECK(parsed.has_value());
    CHECK_EQ(*parsed, path);
}

TEST_CASE("adb_config: serialize escapes backslashes and quotes") {
    const std::string json = adb_config::serialize("a\\b\"c");
    // Backslash -> \\, quote -> \" in the emitted JSON.
    CHECK(json.find("a\\\\b\\\"c") != std::string::npos);
}

TEST_CASE("adb_config: round-trips a path with spaces") {
    const std::string path = "C:\\Program Files\\platform-tools\\adb.exe";
    const auto parsed = adb_config::parseAdbPath(adb_config::serialize(path));
    CHECK(parsed.has_value());
    CHECK_EQ(*parsed, path);
}

TEST_CASE("adb_config: round-trips a path containing a quote") {
    const std::string path = "C:\\weird\"name\\adb.exe";
    const auto parsed = adb_config::parseAdbPath(adb_config::serialize(path));
    CHECK(parsed.has_value());
    CHECK_EQ(*parsed, path);
}

TEST_CASE("adb_config: parse rejects missing key / empty / malformed") {
    CHECK(!adb_config::parseAdbPath("{}").has_value());
    CHECK(!adb_config::parseAdbPath("{\"adbPath\": \"\"}").has_value());
    CHECK(!adb_config::parseAdbPath("not json at all").has_value());
    CHECK(!adb_config::parseAdbPath("").has_value());
}

TEST_CASE("adb_config: parse ignores non-string adbPath") {
    CHECK(!adb_config::parseAdbPath("{\"adbPath\": 5}").has_value());
}

TEST_CASE("adb_config: parse reads adbPath alongside other keys") {
    const auto parsed =
        adb_config::parseAdbPath("{\"other\": 1, \"adbPath\": \"C:\\\\x\\\\adb.exe\"}");
    CHECK(parsed.has_value());
    CHECK_EQ(*parsed, std::string("C:\\x\\adb.exe"));
}
