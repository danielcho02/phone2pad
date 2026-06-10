// Packet encoders: struct -> wire bytes (little-endian). See docs/02-PROTOCOL.md.
#pragma once

#include <cstdint>
#include <vector>

#include "phantompad/proto/haptic.hpp"
#include "phantompad/proto/hello.hpp"
#include "phantompad/proto/ping_pong.hpp"
#include "phantompad/proto/touch_frame.hpp"

namespace phantompad::proto {

using Bytes = std::vector<std::uint8_t>;

Bytes encode(const Hello& hello);
Bytes encode(const TouchFrame& frame);
Bytes encode_ping(const PingPong& ping);
Bytes encode_pong(const PingPong& pong);
Bytes encode(const Haptic& haptic);

}  // namespace phantompad::proto
