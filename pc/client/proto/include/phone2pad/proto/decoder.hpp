// Stream decoder: length-prefixed packet stream -> decoded packets, with
// resynchronization on corruption. Rules: docs/02-PROTOCOL.md §7.3.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include "phone2pad/proto/haptic.hpp"
#include "phone2pad/proto/hello.hpp"
#include "phone2pad/proto/ping_pong.hpp"
#include "phone2pad/proto/touch_frame.hpp"

namespace phone2pad::proto {

// Ping/Pong share a payload layout, so wrap them to preserve the type tag.
struct PingPacket {
    PingPong value;
    friend bool operator==(const PingPacket&, const PingPacket&) = default;
};
struct PongPacket {
    PingPong value;
    friend bool operator==(const PongPacket&, const PongPacket&) = default;
};

using DecodedPacket = std::variant<Hello, TouchFrame, PingPacket, PongPacket, Haptic>;

// Accumulates bytes across feed() calls; emits packets as they complete.
// Skipped (resynced-past) bytes are counted and exposed for tests.
class StreamDecoder {
public:
    // Append bytes and return any packets that became complete. Resyncs past
    // corruption per docs/02 §7.3, incrementing skippedByteCount().
    std::vector<DecodedPacket> feed(std::span<const std::uint8_t> data);

    std::size_t skippedByteCount() const { return skipped_; }
    std::size_t bufferedByteCount() const { return buffer_.size() - cursor_; }

private:
    std::vector<std::uint8_t> buffer_;
    std::size_t cursor_ = 0;  // index of next unconsumed byte in buffer_
    std::size_t skipped_ = 0;

    void compact();
};

}  // namespace phone2pad::proto
