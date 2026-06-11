// Shared helpers for vector-driven tests: hex conversion, vector-file discovery,
// and conversion between the JSON `decoded` object form and proto structs.
#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "json_lite.hpp"
#include "phone2pad/proto/decoder.hpp"
#include "phone2pad/proto/encoder.hpp"
#include "phone2pad/proto/packet.hpp"

namespace phone2pad::testsupport {

namespace fs = std::filesystem;
using proto::Bytes;

inline Bytes from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::runtime_error("odd-length hex string");
    Bytes out;
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("invalid hex digit");
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return out;
}

inline std::string to_hex(const Bytes& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (std::uint8_t b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

inline std::string read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::vector<fs::path> vector_files() {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(PHONE2PAD_TEST_VECTOR_DIR)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Build packet bytes from a JSON `decoded` object using the C++ encoder under test.
inline Bytes encode_decoded(const json::Value& d) {
    const std::string type = d.at("type").as_string();
    if (type == "HELLO") {
        proto::Hello h;
        h.protocolVersion = static_cast<std::uint8_t>(d.at("protocolVersion").as_int());
        h.screenWidthPx = static_cast<std::uint16_t>(d.at("screenWidthPx").as_int());
        h.screenHeightPx = static_cast<std::uint16_t>(d.at("screenHeightPx").as_int());
        h.xdpiX10 = static_cast<std::uint16_t>(d.at("xdpiX10").as_int());
        h.ydpiX10 = static_cast<std::uint16_t>(d.at("ydpiX10").as_int());
        h.rotation = static_cast<std::uint8_t>(d.at("rotation").as_int());
        h.maxContacts = static_cast<std::uint8_t>(d.at("maxContacts").as_int());
        return proto::encode(h);
    }
    if (type == "TOUCH_FRAME") {
        proto::TouchFrame f;
        f.timestampUs = static_cast<std::uint32_t>(d.at("timestampUs").as_int());
        for (const auto& c : d.at("contacts").as_array()) {
            proto::Contact contact;
            contact.id = static_cast<std::uint8_t>(c.at("id").as_int());
            contact.tip = c.at("tip").as_bool();
            contact.confidence = c.at("confidence").as_bool();
            contact.x = static_cast<std::uint16_t>(c.at("x").as_int());
            contact.y = static_cast<std::uint16_t>(c.at("y").as_int());
            f.contacts.push_back(contact);
        }
        return proto::encode(f);
    }
    if (type == "PING") {
        return proto::encode_ping({static_cast<std::uint32_t>(d.at("seq").as_int()),
                                   static_cast<std::uint32_t>(d.at("senderTimestampUs").as_int())});
    }
    if (type == "PONG") {
        return proto::encode_pong({static_cast<std::uint32_t>(d.at("seq").as_int()),
                                   static_cast<std::uint32_t>(d.at("senderTimestampUs").as_int())});
    }
    if (type == "HAPTIC") {
        return proto::encode(proto::Haptic{static_cast<std::uint8_t>(d.at("effect").as_int())});
    }
    throw std::runtime_error("unknown decoded type: " + type);
}

// Does a decoded packet variant match the JSON `decoded` description? Returns a
// human-readable mismatch reason, or empty string on match.
inline std::string decoded_mismatch(const proto::DecodedPacket& pkt, const json::Value& d) {
    const std::string type = d.at("type").as_string();
    if (type == "HELLO") {
        if (!std::holds_alternative<proto::Hello>(pkt)) return "expected HELLO";
        const auto& h = std::get<proto::Hello>(pkt);
        proto::Hello e;
        e.protocolVersion = static_cast<std::uint8_t>(d.at("protocolVersion").as_int());
        e.screenWidthPx = static_cast<std::uint16_t>(d.at("screenWidthPx").as_int());
        e.screenHeightPx = static_cast<std::uint16_t>(d.at("screenHeightPx").as_int());
        e.xdpiX10 = static_cast<std::uint16_t>(d.at("xdpiX10").as_int());
        e.ydpiX10 = static_cast<std::uint16_t>(d.at("ydpiX10").as_int());
        e.rotation = static_cast<std::uint8_t>(d.at("rotation").as_int());
        e.maxContacts = static_cast<std::uint8_t>(d.at("maxContacts").as_int());
        return h == e ? "" : "HELLO fields differ";
    }
    if (type == "TOUCH_FRAME") {
        if (!std::holds_alternative<proto::TouchFrame>(pkt)) return "expected TOUCH_FRAME";
        const auto& f = std::get<proto::TouchFrame>(pkt);
        proto::TouchFrame e;
        e.timestampUs = static_cast<std::uint32_t>(d.at("timestampUs").as_int());
        for (const auto& c : d.at("contacts").as_array()) {
            proto::Contact contact;
            contact.id = static_cast<std::uint8_t>(c.at("id").as_int());
            contact.tip = c.at("tip").as_bool();
            contact.confidence = c.at("confidence").as_bool();
            contact.x = static_cast<std::uint16_t>(c.at("x").as_int());
            contact.y = static_cast<std::uint16_t>(c.at("y").as_int());
            e.contacts.push_back(contact);
        }
        return f == e ? "" : "TOUCH_FRAME fields differ";
    }
    if (type == "PING") {
        if (!std::holds_alternative<proto::PingPacket>(pkt)) return "expected PING";
        const auto& p = std::get<proto::PingPacket>(pkt).value;
        return (p.seq == d.at("seq").as_int() &&
                p.senderTimestampUs == d.at("senderTimestampUs").as_int())
                   ? "" : "PING fields differ";
    }
    if (type == "PONG") {
        if (!std::holds_alternative<proto::PongPacket>(pkt)) return "expected PONG";
        const auto& p = std::get<proto::PongPacket>(pkt).value;
        return (p.seq == d.at("seq").as_int() &&
                p.senderTimestampUs == d.at("senderTimestampUs").as_int())
                   ? "" : "PONG fields differ";
    }
    if (type == "HAPTIC") {
        if (!std::holds_alternative<proto::Haptic>(pkt)) return "expected HAPTIC";
        return std::get<proto::Haptic>(pkt).effect == d.at("effect").as_int()
                   ? "" : "HAPTIC effect differs";
    }
    return "unknown decoded type: " + type;
}

}  // namespace phone2pad::testsupport
