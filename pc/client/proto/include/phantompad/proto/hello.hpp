// HELLO (0x01) payload — see docs/02-PROTOCOL.md §3.
#pragma once

#include <cstdint>

namespace phantompad::proto {

struct Hello {
    std::uint8_t protocolVersion = 1;
    std::uint16_t screenWidthPx = 0;   // Activity-frame reference (Phase A=landscape)
    std::uint16_t screenHeightPx = 0;
    std::uint16_t xdpiX10 = 0;         // xdpi * 10
    std::uint16_t ydpiX10 = 0;
    std::uint8_t rotation = 0;         // 0=0, 1=90, 2=180, 3=270
    std::uint8_t maxContacts = 0;

    friend bool operator==(const Hello&, const Hello&) = default;
};

}  // namespace phantompad::proto
