#include "phone2pad/client/adb_config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace phone2pad::client::adb_config {

namespace {

std::optional<std::string> envVar(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0') return std::nullopt;
    return std::string(v);
}

// ---- minimal, self-contained JSON for the single {"adbPath": "..."} shape ----
// Deliberately tiny and dependency-free (production code must not pull in the test-only
// json_lite.hpp). Only what parseAdbPath needs: JSON string literals, plus enough
// structure-skipping to find the adbPath key among other keys.

void skipWs(const std::string& s, std::size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
        ++i;
    }
}

// Parse a JSON string literal at s[i] (which must be the opening '"'). On success, fill
// `out` with the unescaped value, advance i past the closing quote, and return true.
// Unescapes \\ and \" (the pair serialize() emits) plus the common \/ \n \t \r \b \f;
// rejects unterminated strings and unsupported escapes.
bool parseString(const std::string& s, std::size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;  // consume opening quote
    out.clear();
    while (i < s.size()) {
        const char c = s[i++];
        if (c == '"') return true;
        if (c == '\\') {
            if (i >= s.size()) return false;  // dangling escape
            const char e = s[i++];
            switch (e) {
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '/': out.push_back('/'); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                default: return false;  // unsupported escape
            }
        } else {
            out.push_back(c);
        }
    }
    return false;  // unterminated string
}

// Skip a single JSON value (string / number / bool / null / nested object or array) so
// we can step over keys we don't care about. Returns false on malformed input.
bool skipValue(const std::string& s, std::size_t& i) {
    skipWs(s, i);
    if (i >= s.size()) return false;
    const char c = s[i];
    if (c == '"') {
        std::string tmp;
        return parseString(s, i, tmp);
    }
    if (c == '{' || c == '[') {
        // Depth-count nested brackets, respecting string literals so a '}' inside a
        // string doesn't close the structure.
        int depth = 0;
        while (i < s.size()) {
            const char d = s[i];
            if (d == '"') {
                std::string tmp;
                if (!parseString(s, i, tmp)) return false;
                continue;
            }
            if (d == '{' || d == '[') {
                ++depth;
            } else if (d == '}' || d == ']') {
                --depth;
                ++i;
                if (depth == 0) return true;
                continue;
            }
            ++i;
        }
        return false;  // unbalanced
    }
    // Primitive (number / true / false / null): consume until a delimiter.
    while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' &&
           s[i] != '\t' && s[i] != '\n' && s[i] != '\r') {
        ++i;
    }
    return true;
}

}  // namespace

std::string serialize(const std::string& adbPath) {
    std::string escaped;
    escaped.reserve(adbPath.size() + 16);
    for (char c : adbPath) {
        if (c == '\\' || c == '"') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return "{\"adbPath\": \"" + escaped + "\"}\n";
}

std::optional<std::string> parseAdbPath(const std::string& json) {
    std::size_t i = 0;
    skipWs(json, i);
    if (i >= json.size() || json[i] != '{') return std::nullopt;
    ++i;  // consume '{'
    skipWs(json, i);
    if (i < json.size() && json[i] == '}') return std::nullopt;  // empty object

    for (;;) {
        skipWs(json, i);
        std::string key;
        if (!parseString(json, i, key)) return std::nullopt;  // expected a key
        skipWs(json, i);
        if (i >= json.size() || json[i] != ':') return std::nullopt;
        ++i;  // consume ':'
        skipWs(json, i);

        if (key == "adbPath") {
            if (i >= json.size() || json[i] != '"') return std::nullopt;  // not a string
            std::string value;
            if (!parseString(json, i, value)) return std::nullopt;
            if (value.empty()) return std::nullopt;
            return value;
        }

        if (!skipValue(json, i)) return std::nullopt;  // step over an unrelated value
        skipWs(json, i);
        if (i >= json.size()) return std::nullopt;
        if (json[i] == ',') {
            ++i;
            continue;
        }
        return std::nullopt;  // '}' (no adbPath) or malformed
    }
}

std::string configFilePath() {
    namespace fs = std::filesystem;
    auto localAppData = envVar("LOCALAPPDATA");
    if (!localAppData) return std::string();
    return (fs::path(*localAppData) / "phone2pad" / "config.json").string();
}

std::optional<std::string> loadAdbPath() {
    const std::string path = configFilePath();
    if (path.empty()) return std::nullopt;
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseAdbPath(ss.str());
}

bool saveAdbPath(const std::string& adbPath) {
    namespace fs = std::filesystem;
    const std::string path = configFilePath();
    if (path.empty()) return false;

    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    // create_directories returns false (with no error) when the dir already exists, so
    // rely on ec, not the return value.
    if (ec) return false;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << serialize(adbPath);
    return static_cast<bool>(out);
}

std::optional<std::string> adbExeUnder(const std::string& folder) {
    namespace fs = std::filesystem;
    if (folder.empty()) return std::nullopt;

    const fs::path base(folder);
    const fs::path candidates[] = {
        base / "adb.exe",
        base / "platform-tools" / "adb.exe",
    };
    for (const fs::path& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) return c.string();
    }
    return std::nullopt;
}

}  // namespace phone2pad::client::adb_config
