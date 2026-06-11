// PING (0x03) / PONG (0x04) payload — see docs/02-PROTOCOL.md §5.
// Identical wire layout; the surrounding packet type distinguishes direction.
#pragma once

#include <cstdint>

namespace phone2pad::proto {

struct PingPong {
    std::uint32_t seq = 0;
    std::uint32_t senderTimestampUs = 0;

    friend bool operator==(const PingPong&, const PingPong&) = default;
};

}  // namespace phone2pad::proto
