// .pptrace I/O (docs/05 §3): JSON Lines, one TouchFrame per line, human-readable.
//   {"t": 16666, "contacts": [{"id":0,"tip":1,"x":510,"y":1198}]}
// Used by the recorder (write) and replay (read) tools to develop/test the PC side
// without a phone.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "phantompad/proto/touch_frame.hpp"

namespace phantompad::client {

struct TimedFrame {
    std::uint32_t t = 0;  // microseconds on the original timeline
    proto::TouchFrame frame;
};

// Parse a .pptrace file. Throws std::runtime_error on I/O or malformed input.
std::vector<TimedFrame> readTrace(const std::string& path);

// One .pptrace line (no trailing newline) for the given frame at time t.
std::string toTraceLine(std::uint32_t t, const proto::TouchFrame& frame);

}  // namespace phantompad::client
