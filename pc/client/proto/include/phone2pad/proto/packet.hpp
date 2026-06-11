// phone2pad wire protocol — common header and packet-type definitions.
// Single source of truth: docs/02-PROTOCOL.md. Byte order is little-endian.
#pragma once

#include <cstddef>
#include <cstdint>

namespace phone2pad::proto {

inline constexpr std::uint8_t kMagic = 0xA7;

// Common header is 4 bytes: magic(u8) | type(u8) | length(u16, payload length).
inline constexpr std::size_t kHeaderSize = 4;

enum class PacketType : std::uint8_t {
    Hello = 0x01,
    TouchFrame = 0x02,
    Ping = 0x03,
    Pong = 0x04,
    Haptic = 0x05,
};

// Contact flag bits (TOUCH_FRAME contact.flags).
inline constexpr std::uint8_t kFlagTip = 0x01;        // 1 = touching, 0 = lifting
inline constexpr std::uint8_t kFlagConfidence = 0x02; // 1 = normal, 0 = palm-suspect

// Fixed payload sizes / bounds used for stream validation (docs/02 §7.3).
inline constexpr std::size_t kHelloPayloadSize = 11;
inline constexpr std::size_t kPingPongPayloadSize = 8;
inline constexpr std::size_t kHapticPayloadSize = 1;
inline constexpr std::size_t kTouchFrameHeaderSize = 5; // timestampUs(4) + contactCount(1)
inline constexpr std::size_t kContactSize = 6;          // id(1)+flags(1)+x(2)+y(2)
inline constexpr std::size_t kMaxContacts = 16;

inline constexpr bool is_valid_type(std::uint8_t type) {
    return type >= static_cast<std::uint8_t>(PacketType::Hello) &&
           type <= static_cast<std::uint8_t>(PacketType::Haptic);
}

}  // namespace phone2pad::proto
