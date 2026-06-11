// MouseSink (Phase A + Phase B click/drag): single-finger relative mouse move,
// tap-to-click, and press-and-hold drag via an InputInjector. Multi-touch
// (contactCount != 1) freezes the cursor and resets the movement anchor so re-touch
// does not jump. 2+ finger gestures are handled by GestureSink.
//
// A single-finger session runs a small state machine: a finger held (nearly) still past
// longPressDragUs arms a left-button drag (leftDown), movement then drags with the button
// held, and lift/cancel/disconnect always releases it (leftUp). A finger that travels past
// dragStartMovePx before the timer is an ordinary cursor move and never arms a drag.
#pragma once

#include <cstdint>

#include "phone2pad/client/input_injector.hpp"
#include "phone2pad/client/sink.hpp"

namespace phone2pad::client {

struct MouseSinkConfig {
    double sensitivity = 1.0;           // multiplier on raw pixel deltas
    std::uint32_t tapMaxUs = 180'000;   // max press duration to count as a tap
    int tapMovePx = 12;                 // max travel from press point to count as a tap
    // Press-and-hold drag: a single finger held within dragStartMovePx of its press point
    // for at least longPressDragUs arms a left-button drag (350-500ms is a comfortable
    // range). dragStartMovePx is the "held still enough" tolerance, kept separate from the
    // tighter tap travel limit so finger jitter does not cancel the arming.
    std::uint32_t longPressDragUs = 400'000;
    int dragStartMovePx = 16;
};

class MouseSink : public Sink {
public:
    explicit MouseSink(InputInjector& injector, MouseSinkConfig config = {});

    void onFrame(const proto::TouchFrame& frame) override;

private:
    // Single-finger session lifecycle (docs/04 Phase B click/drag).
    enum class DragState {
        Idle,           // no single-finger session in progress
        OneFingerDown,  // finger down, still inside the tap/arm window
        Moving,         // travelled past dragStartMovePx before the timer: plain cursor move
        DragArmed,      // long-press fired: leftDown held, no drag move yet
        Dragging,       // leftDown held and the cursor is being dragged
    };

    void beginContact(const proto::Contact& c, std::uint32_t tsUs);
    void endContactAsTap(std::uint32_t tsUs);
    void releaseDragIfHeld();  // emit leftUp when a drag is armed/active (idempotent)

    InputInjector& inj_;
    MouseSinkConfig cfg_;

    // Movement anchor (last single-contact position). Valid while not Idle.
    int lastX_ = 0;
    int lastY_ = 0;
    double accumX_ = 0.0;   // sub-pixel carry so sensitivity != 1.0 stays smooth
    double accumY_ = 0.0;

    // Single-finger contact session (for tap and long-press detection).
    DragState state_ = DragState::Idle;
    std::uint32_t downTimeUs_ = 0;
    int downX_ = 0;
    int downY_ = 0;
    int maxTravelPx_ = 0;
};

}  // namespace phone2pad::client
