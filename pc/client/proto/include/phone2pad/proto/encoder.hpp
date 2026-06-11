// Packet encoders: struct -> wire bytes (little-endian). See docs/02-PROTOCOL.md.
#pragma once

#include <cstdint>
#include <vector>

#include "phone2pad/proto/haptic.hpp"
#include "phone2pad/proto/hello.hpp"
#include "phone2pad/proto/ping_pong.hpp"
#include "phone2pad/proto/touch_frame.hpp"

namespace phone2pad::proto {

using Bytes = std::vector<std::uint8_t>;

Bytes encode(const Hello& hello);
Bytes encode(const TouchFrame& frame);
Bytes encode_ping(const PingPong& ping);
Bytes encode_pong(const PingPong& pong);
Bytes encode(const Haptic& haptic);

}  // namespace phone2pad::proto
