// MouseSink (Phase A): single-finger relative mouse move + tap-to-click via an
// InputInjector. Multi-touch (contactCount != 1) freezes the cursor and resets the
// movement anchor so re-touch does not jump. 2+ finger gestures are Phase B.
#pragma once

#include <cstdint>

#include "phantompad/client/input_injector.hpp"
#include "phantompad/client/sink.hpp"

namespace phantompad::client {

struct MouseSinkConfig {
    double sensitivity = 1.0;       // multiplier on raw pixel deltas
    std::uint32_t tapMaxUs = 180'000;   // max press duration to count as a tap
    int tapMovePx = 12;             // max travel from press point to count as a tap
};

class MouseSink : public Sink {
public:
    explicit MouseSink(InputInjector& injector, MouseSinkConfig config = {});

    void onFrame(const proto::TouchFrame& frame) override;

private:
    void beginContact(const proto::Contact& c, std::uint32_t tsUs);
    void endContactAsTap(std::uint32_t tsUs);

    InputInjector& inj_;
    MouseSinkConfig cfg_;

    // Movement anchor (last single-contact position). Valid while inContact_.
    int lastX_ = 0;
    int lastY_ = 0;
    double accumX_ = 0.0;   // sub-pixel carry so sensitivity != 1.0 stays smooth
    double accumY_ = 0.0;

    // Single-finger contact session (for tap detection).
    bool inContact_ = false;
    std::uint32_t downTimeUs_ = 0;
    int downX_ = 0;
    int downY_ = 0;
    int maxTravelPx_ = 0;
};

}  // namespace phantompad::client
