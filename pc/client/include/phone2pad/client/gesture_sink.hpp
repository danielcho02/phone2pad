// GestureSink (Phase B): interprets 2+ finger sessions into Precision-Touchpad-like
// actions via an InputInjector (docs/01 §2.2, docs/04 Phase B):
//   2 fingers: scroll (wheel/hwheel), tap = right click, pinch = Ctrl+wheel
//   3 fingers: vertical swipe = Win+Tab / Win+D; horizontal = interactive Alt+Tab
//              (hold Alt, step selection with Tab / Shift+Tab, confirm on lift)
//   4 fingers: swipe left/right = Ctrl+Win+Left/Right (virtual desktops)
// A multi-touch "settle window" stabilizes the finger count after the first contact so
// landing skew (3 fingers landing before the 4th) does not commit a 3-finger gesture.
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

    // Interactive Alt+Tab: centroid px per selection step while Alt is held
    int altTabStepPx = 60;

    // Pinch zoom (Phase B2). Toggle off to isolate scroll behaviour.
    bool enablePinch = true;
    int pinchPixelsPerNotch = 40;    // contact-distance change px per Ctrl+wheel notch

    // 4-finger up/down mirrors 3-finger (Task View / Show desktop) when true.
    bool fourFingerVerticalMirror = true;

    // Multi-touch settle window: after the first 2+ contact, wait this long (or until
    // 4 contacts are seen) before locking the session finger count. Absorbs landing
    // skew between fingers. No gesture commits during the window.
    std::uint32_t settleWindowUs = 90'000;
};

class GestureSink : public Sink {
public:
    explicit GestureSink(InputInjector& injector, GestureConfig config = {});

    void onFrame(const proto::TouchFrame& frame) override;

private:
    enum class Mode { None, Scroll, Pinch, Swipe3, Swipe4, AltTab };

    void begin(double cx, double cy, double dist, int count, std::uint32_t tsUs);
    void handleTwo(double cx, double cy, double dist, int activeCount);
    void handleThreeFinger(double cx, double cy);
    void handleFour(double cx, double cy);
    void finalizeOnLift(std::uint32_t tsUs);
    void reset();

    void emitScroll(double dCx, double dCy);
    void emitPinch(double dist);
    void ctrlWheel(int delta);

    // Interactive Alt+Tab.
    void enterAltTab(bool forward);  // forward = right swipe (Tab), else left (Shift+Tab)
    void handleAltTab(double cx);
    void tabStep(bool forward);
    void releaseAltTab();            // keyUp Alt (and Shift) — confirms selection

    InputInjector& inj_;
    GestureConfig cfg_;

    bool active_ = false;            // inside a 2+ finger session
    int peakCount_ = 0;             // max active contacts seen this session (latch up)
    std::uint32_t startUs_ = 0;

    bool settled_ = false;          // finger count locked (settle window over)
    int sessionFingers_ = 0;       // locked finger count (2/3/4) after settle

    double anchorCx_ = 0, anchorCy_ = 0;  // gesture-start centroid (swipe/tap reference)
    double lastCx_ = 0, lastCy_ = 0;      // previous frame centroid (incremental delta)
    double anchorDist_ = 0, lastDist_ = 0;  // 2-finger contact distance
    double accumX_ = 0, accumY_ = 0, accumPinch_ = 0;  // sub-notch carries
    double maxTravelPx_ = 0;        // max centroid travel from anchor (tap rejection)

    bool committed_ = false;        // gesture mode locked for the rest of the session
    Mode mode_ = Mode::None;

    // Alt+Tab session state.
    bool altActive_ = false;        // Alt currently held
    bool shiftHeld_ = false;        // Shift currently held (defensive release on reset)
    double altAnchorX_ = 0;        // centroid x reference for the next selection step
};

}  // namespace phone2pad::client
