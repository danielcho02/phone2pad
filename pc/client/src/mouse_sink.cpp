#include "phone2pad/client/mouse_sink.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace phone2pad::client {

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
        if (state_ == DragState::Idle) {
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
        const bool moved = (mvX != 0 || mvY != 0);
        if (moved) {
            inj_.moveRelative(mvX, mvY);  // a held drag keeps moving with the button down
        }
        lastX_ = c.x;
        lastY_ = c.y;
        const int tx = std::abs(static_cast<int>(c.x) - downX_);
        const int ty = std::abs(static_cast<int>(c.y) - downY_);
        maxTravelPx_ = std::max(maxTravelPx_, std::max(tx, ty));

        // State transitions. Travel past the arm tolerance makes this a plain cursor move
        // (never a drag); otherwise holding past the timer arms a left-button drag. The
        // travel check comes first: at the first move frame after a still hold the travel
        // is still small, so the elapsed branch arms the drag as intended.
        if (state_ == DragState::OneFingerDown) {
            const std::uint32_t elapsedUs = frame.timestampUs - downTimeUs_;  // wrap-safe
            if (maxTravelPx_ > cfg_.dragStartMovePx) {
                state_ = DragState::Moving;
            } else if (elapsedUs >= cfg_.longPressDragUs) {
                state_ = DragState::DragArmed;
                inj_.leftDown();  // emitted exactly once on this edge
            }
        } else if (state_ == DragState::DragArmed && moved) {
            state_ = DragState::Dragging;
        }
        return;
    }

    // 0 or 2+ active contacts: the single-finger session ends here.
    if (state_ != DragState::Idle) {
        if (activeCount == 0 && state_ != DragState::DragArmed &&
            state_ != DragState::Dragging) {
            // Plain lift -> maybe a tap. Two quick taps emit two clicks, which the OS
            // recognizes as a double-click (docs/04 Phase A double-tap).
            endContactAsTap(frame.timestampUs);
        } else {
            // Lift while dragging, or a second finger interrupting: release the button so a
            // drag never sticks. releaseDragIfHeld is a no-op outside a drag, so a plain
            // finger landing into multi-touch just freezes the cursor as before.
            releaseDragIfHeld();
        }
    }
    // Multi-touch (gesture) or full lift: freeze cursor, reset anchor.
    state_ = DragState::Idle;
}

void MouseSink::beginContact(const proto::Contact& c, std::uint32_t tsUs) {
    state_ = DragState::OneFingerDown;
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

void MouseSink::releaseDragIfHeld() {
    if (state_ == DragState::DragArmed || state_ == DragState::Dragging) {
        inj_.leftUp();  // guarantees the held button is released on lift/cancel/disconnect
    }
}

}  // namespace phone2pad::client
