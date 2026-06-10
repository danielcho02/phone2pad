// HAPTIC (0x05) payload — see docs/02-PROTOCOL.md §6 (Phase D).
#pragma once

#include <cstdint>

namespace phantompad::proto {

struct Haptic {
    std::uint8_t effect = 0;  // 0=click, 1=light tick

    friend bool operator==(const Haptic&, const Haptic&) = default;
};

}  // namespace phantompad::proto
