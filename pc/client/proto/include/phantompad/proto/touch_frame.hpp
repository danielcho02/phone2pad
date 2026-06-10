// TOUCH_FRAME (0x02) payload — see docs/02-PROTOCOL.md §4.
#pragma once

#include <cstdint>
#include <vector>

namespace phantompad::proto {

struct Contact {
    std::uint8_t id = 0;        // MotionEvent pointerId (stable within a session)
    bool tip = false;          // flags bit0
    bool confidence = true;    // flags bit1
    std::uint16_t x = 0;       // pixels, Activity-frame absolute coords (Phase A=landscape)
    std::uint16_t y = 0;

    friend bool operator==(const Contact&, const Contact&) = default;
};

struct TouchFrame {
    std::uint32_t timestampUs = 0;  // phone monotonic clock, us, wrap allowed
    std::vector<Contact> contacts;

    friend bool operator==(const TouchFrame&, const TouchFrame&) = default;
};

}  // namespace phantompad::proto
