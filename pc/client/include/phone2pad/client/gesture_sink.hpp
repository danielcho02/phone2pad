// GestureSink (Phase B): interprets 2+ finger sessions into Precision-Touchpad-like
// actions via an InputInjector (docs/01 §2.2, docs/04 Phase B):
//   2 fingers: scroll (wheel/hwheel), tap = right click, pinch = Ctrl+wheel
//   3 fingers: swipe up/down/left/right -> Win+Tab / Win+D / Alt(+Shift)+Tab
//   4 fingers: swipe left/right -> Ctrl+Win+Left/Right (virtual desktops)
// Single-finger sessions never reach here; GestureRouter routes those to MouseSink.
// The phone stays a raw sensor — all gesture detection lives here.
#pragma once

#include <cstdint>

#include "phone2pad/client/input_injector.hpp"
#include "phone2pad/client/sink.hpp"

namespace phone2pad::client {

struct GestureConfig {
    // 2-finger scroll
    bool naturalScroll = true;       // finger direction follows content (phone-like)
    int scrollPixelsPerNotch = 30;   // centroid px per WHEEL_DELTA notch

    // 2-finger tap -> right click
    std::uint32_t tapMaxUs = 180'000;  // max press duration to count as a tap
    int tapMovePx = 12;                // max centroid travel to count as a tap

    // 3/4-finger swipe: centroid travel (px) before the gesture commits
    int swipeCommitPx = 80;

    // Pinch zoom (Phase B2). Toggle off to isolate scroll behaviour.
    bool enablePinch = true;
    int pinchPixelsPerNotch = 40;    // contact-distance change px per Ctrl+wheel notch

    // 4-finger up/down mirrors 3-finger (Task View / Show desktop) when true.
    bool fourFingerVerticalMirror = true;
};

class GestureSink : public Sink {
public:
    explicit GestureSink(InputInjector& injector, GestureConfig config = {});

    void onFrame(const proto::TouchFrame& frame) override;

private:
    enum class Mode { None, Scroll, Pinch, Swipe3, Swipe4 };

    void begin(double cx, double cy, double dist, int count, std::uint32_t tsUs);
    void handleTwo(double cx, double cy, double dist, int activeCount);
    void handleSwipe(double cx, double cy, int fingers);
    void finalizeOnLift(std::uint32_t tsUs);
    void reset();

    void emitScroll(double dCx, double dCy);
    void emitPinch(double dist);
    void ctrlWheel(int delta);

    InputInjector& inj_;
    GestureConfig cfg_;

    bool active_ = false;            // inside a 2+ finger session
    int peakCount_ = 0;             // max active contacts seen this session (latch up)
    std::uint32_t startUs_ = 0;

    double anchorCx_ = 0, anchorCy_ = 0;  // session-start centroid (swipe/tap reference)
    double lastCx_ = 0, lastCy_ = 0;      // previous frame centroid (incremental delta)
    double anchorDist_ = 0, lastDist_ = 0;  // 2-finger contact distance
    double accumX_ = 0, accumY_ = 0, accumPinch_ = 0;  // sub-notch carries
    double maxTravelPx_ = 0;        // max centroid travel from anchor (tap rejection)

    bool committed_ = false;        // gesture mode locked for the rest of the session
    Mode mode_ = Mode::None;
};

}  // namespace phone2pad::client
