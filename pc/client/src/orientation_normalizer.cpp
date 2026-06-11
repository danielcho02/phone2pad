#include "phone2pad/client/orientation_normalizer.hpp"

#include <algorithm>

namespace phone2pad::client {

namespace {
constexpr int kRotation270 = 3;  // HELLO rotation value for ROTATION_270 (flipped landscape)
}  // namespace

proto::TouchFrame normalizeFrame(const proto::TouchFrame& frame, int rotation, int widthPx,
                                 int heightPx) {
    if (rotation != kRotation270 || widthPx <= 0 || heightPx <= 0) {
        return frame;  // canonical landscape (or unknown dims): identity
    }
    proto::TouchFrame out = frame;
    for (auto& c : out.contacts) {
        const int fx = std::clamp(widthPx - 1 - static_cast<int>(c.x), 0, widthPx - 1);
        const int fy = std::clamp(heightPx - 1 - static_cast<int>(c.y), 0, heightPx - 1);
        c.x = static_cast<std::uint16_t>(fx);
        c.y = static_cast<std::uint16_t>(fy);
    }
    return out;
}

}  // namespace phone2pad::client
