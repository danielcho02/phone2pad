// User-local config for phone2pad (v0.3.0): a single setting, the user-selected
// adb.exe path. Stored as JSON at %LOCALAPPDATA%\phone2pad\config.json so a
// non-developer can point the app at an extracted platform-tools folder via the tray
// folder picker instead of dropping files next to the exe (see docs/ADB-SETUP.md).
//
// Deliberately tiny — NOT a general settings system. Only `adbPath` is supported.
//
// The (de)serialization is split into pure helpers so it can be unit-tested without
// touching the real filesystem; the load/save wrappers are thin filesystem glue.
#pragma once

#include <optional>
#include <string>

namespace phone2pad::client::adb_config {

// ---- pure helpers (no filesystem access) ----

// Serialize a config holding a single adb.exe path to JSON:
//   {"adbPath": "<escaped path>"}
// Backslashes and quotes are escaped so the json_lite reader round-trips it exactly.
std::string serialize(const std::string& adbPath);

// Read the "adbPath" string from config JSON. Returns nullopt when the key is absent,
// empty, or the input is malformed (the parser throws; we swallow it). Paths are stored
// and returned as narrow (active code page) strings, matching AdbManager's convention.
std::optional<std::string> parseAdbPath(const std::string& json);

// ---- filesystem wrappers ----

// Absolute path of the config file (%LOCALAPPDATA%\phone2pad\config.json). Empty when
// %LOCALAPPDATA% is unset.
std::string configFilePath();

// Load the configured adb.exe path, if a valid config exists. Does not verify the path
// still exists on disk — adb discovery (AdbManager) does that.
std::optional<std::string> loadAdbPath();

// Persist the configured adb.exe path, creating %LOCALAPPDATA%\phone2pad if needed.
// Returns true on success.
bool saveAdbPath(const std::string& adbPath);

// Validate a user-picked folder and return the adb.exe inside it, if present. Checks
// <folder>\adb.exe first, then (as a friendly fallback for the common "selected the
// parent" mistake) <folder>\platform-tools\adb.exe. Returns nullopt when neither exists.
std::optional<std::string> adbExeUnder(const std::string& folder);

}  // namespace phone2pad::client::adb_config
