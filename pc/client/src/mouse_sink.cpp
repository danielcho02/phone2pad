#include "phantompad/client/mouse_sink.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace phantompad::client {

MouseSink::MouseSink(InputInjector& injector, MouseSinkConfig config)
    : inj_(injector), cfg_(config) {}

void MouseSink::onFrame(const proto::TouchFrame& frame) {
    // Active = currently touching (tip). A lifting contact reports tip=0 once.
    const proto::Contact* active = nullptr;
    int activeCount = 0;
    for (const auto& c : frame.contacts) {
        if (c.tip) {
            ++activeCount;
            active = &c;
        }
    }

    if (activeCount == 1) {
        const proto::Contact& c = *active;
        if (!inContact_) {
            // First frame of a single-finger session: anchor only, no move (avoids
            // a jump when resuming from multi-touch or after a lift).
            beginContact(c, frame.timestampUs);
            return;
        }
        const int dxRaw = static_cast<int>(c.x) - lastX_;
        const int dyRaw = static_cast<int>(c.y) - lastY_;
        accumX_ += dxRaw * cfg_.sensitivity;
        accumY_ += dyRaw * cfg_.sensitivity;
        const int mvX = static_cast<int>(accumX_);  // truncates toward zero
        const int mvY = static_cast<int>(accumY_);
        accumX_ -= mvX;
        accumY_ -= mvY;
        if (mvX != 0 || mvY != 0) {
            inj_.moveRelative(mvX, mvY);
        }
        lastX_ = c.x;
        lastY_ = c.y;
        const int tx = std::abs(static_cast<int>(c.x) - downX_);
        const int ty = std::abs(static_cast<int>(c.y) - downY_);
        maxTravelPx_ = std::max(maxTravelPx_, std::max(tx, ty));
        return;
    }

    // 0 or 2+ active contacts.
    if (inContact_ && activeCount == 0) {
        // Single finger lifted -> maybe a tap. Two quick taps emit two clicks, which
        // the OS recognizes as a double-click (docs/04 Phase A double-tap).
        endContactAsTap(frame.timestampUs);
    }
    // Multi-touch (gesture, Phase B) or full lift: freeze cursor, reset anchor.
    inContact_ = false;
}

void MouseSink::beginContact(const proto::Contact& c, std::uint32_t tsUs) {
    inContact_ = true;
    lastX_ = c.x;
    lastY_ = c.y;
    accumX_ = 0.0;
    accumY_ = 0.0;
    downTimeUs_ = tsUs;
    downX_ = c.x;
    downY_ = c.y;
    maxTravelPx_ = 0;
}

void MouseSink::endContactAsTap(std::uint32_t tsUs) {
    const std::uint32_t durUs = tsUs - downTimeUs_;  // u32 wrap-safe
    if (durUs <= cfg_.tapMaxUs && maxTravelPx_ <= cfg_.tapMovePx) {
        inj_.leftClick();
    }
}

}  // namespace phantompad::client
