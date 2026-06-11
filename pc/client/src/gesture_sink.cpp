#include "phone2pad/client/gesture_sink.hpp"

#include <algorithm>
#include <cmath>

namespace phone2pad::client {

namespace {

constexpr int kWheelDelta = 120;  // Win32 WHEEL_DELTA (one detent)

// Win32 virtual-key codes (kept local so the gesture logic stays free of <windows.h>
// and remains unit-testable). Values per the Microsoft VK_* documentation.
// https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes
namespace vk {
constexpr int TAB = 0x09;
constexpr int SHIFT = 0x10;
constexpr int CTRL = 0x11;
constexpr int ALT = 0x12;   // VK_MENU
constexpr int LEFT = 0x25;
constexpr int RIGHT = 0x27;
constexpr int D = 0x44;
constexpr int LWIN = 0x5B;
}  // namespace vk

struct Active {
    int count = 0;
    double cx = 0, cy = 0;  // centroid of active contacts
    double dist = 0;        // distance between the two contacts (count == 2 only)
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

Active gather(const proto::TouchFrame& frame) {
    Active a;
    double sx = 0, sy = 0;
    for (const auto& c : frame.contacts) {
        if (!c.tip) continue;
        if (a.count == 0) {
            a.x0 = c.x;
            a.y0 = c.y;
        } else if (a.count == 1) {
            a.x1 = c.x;
            a.y1 = c.y;
        }
        sx += c.x;
        sy += c.y;
        ++a.count;
    }
    if (a.count > 0) {
        a.cx = sx / a.count;
        a.cy = sy / a.count;
    }
    if (a.count == 2) {
        a.dist = std::hypot(a.x1 - a.x0, a.y1 - a.y0);
    }
    return a;
}

double maxAxis(double dx, double dy) { return std::max(std::abs(dx), std::abs(dy)); }

}  // namespace

GestureSink::GestureSink(InputInjector& injector, GestureConfig config)
    : inj_(injector), cfg_(config) {}

void GestureSink::onFrame(const proto::TouchFrame& frame) {
    const Active a = gather(frame);

    if (!active_) {
        if (a.count >= 2) begin(a.cx, a.cy, a.dist, a.count, frame.timestampUs);
        return;  // first frame anchors only, never acts
    }

    if (a.count == 0) {
        finalizeOnLift(frame.timestampUs);
        reset();
        return;
    }

    peakCount_ = std::max(peakCount_, a.count);
    // Tap travel is only meaningful for a clean 2-finger session: when a finger is mid-land
    // or mid-lift the centroid jumps to a single contact (tens of px). maxTravelPx_ feeds
    // only the 2-finger tap (3/4-finger swipes use anchorCx_/anchorCy_ directly), so guard
    // it to count==2 frames to keep a tap with landing/lift skew from inflating it.
    if (a.count == 2) {
        maxTravelPx_ = std::max(maxTravelPx_, maxAxis(a.cx - anchorCx_, a.cy - anchorCy_));
    }

    if (!settled_) {
        // Lock the finger count once 4 are seen (can't grow further) or the settle
        // window elapses. Re-anchor to the current centroid so the landing-skew motion
        // during the window is not counted as gesture delta.
        const bool windowElapsed = (frame.timestampUs - startUs_) >= cfg_.settleWindowUs;
        if (peakCount_ >= 4 || windowElapsed) {
            settled_ = true;
            sessionFingers_ = std::min(peakCount_, 4);
            anchorCx_ = lastCx_ = a.cx;
            anchorCy_ = lastCy_ = a.cy;
            anchorDist_ = lastDist_ = a.dist;
            accumX_ = accumY_ = accumPinch_ = 0.0;
            maxTravelPx_ = 0.0;
        }
        return;  // no gesture commit while settling
    }

    if (sessionFingers_ >= 4) {
        handleFour(a.cx, a.cy);
    } else if (sessionFingers_ == 3) {
        handleThreeFinger(a.cx, a.cy);
    } else {
        handleTwo(a.cx, a.cy, a.dist, a.count);
    }
}

void GestureSink::begin(double cx, double cy, double dist, int count, std::uint32_t tsUs) {
    active_ = true;
    peakCount_ = count;
    startUs_ = tsUs;
    settled_ = false;
    sessionFingers_ = 0;
    anchorCx_ = lastCx_ = cx;
    anchorCy_ = lastCy_ = cy;
    anchorDist_ = lastDist_ = dist;
    accumX_ = accumY_ = accumPinch_ = 0.0;
    maxTravelPx_ = 0.0;
    committed_ = false;
    mode_ = Mode::None;
    altActive_ = false;
    shiftHeld_ = false;
    altAnchorX_ = 0.0;
}

void GestureSink::handleTwo(double cx, double cy, double dist, int activeCount) {
    // Only act on clean two-contact frames. A frame with one finger mid-land or mid-lift
    // has a centroid that has jumped to a single contact; acting on it would falsely commit
    // a scroll and suppress the right-click of a two-finger tap with landing/lift skew.
    if (activeCount != 2) return;

    if (!committed_) {
        const double move = maxAxis(cx - anchorCx_, cy - anchorCy_);
        const double dChg = std::abs(dist - anchorDist_);
        const bool pinchOk = cfg_.enablePinch;
        if (move > cfg_.scrollCommitPx || (pinchOk && dChg > cfg_.scrollCommitPx)) {
            mode_ = (pinchOk && dChg > move) ? Mode::Pinch : Mode::Scroll;
            committed_ = true;
            // Re-anchor incremental state at commit so the dead-zone travel before
            // the decision is not dumped as one big step.
            lastCx_ = cx;
            lastCy_ = cy;
            lastDist_ = dist;
            accumX_ = accumY_ = accumPinch_ = 0.0;
        }
        return;
    }

    if (mode_ == Mode::Scroll) {
        emitScroll(cx - lastCx_, cy - lastCy_);
        lastCx_ = cx;
        lastCy_ = cy;
    } else if (mode_ == Mode::Pinch) {  // activeCount == 2 guaranteed above
        emitPinch(dist);
    }
}

void GestureSink::emitScroll(double dCx, double dCy) {
    const int ppn = std::max(1, cfg_.scrollPixelsPerNotch);
    // Vertical: finger up (screen y decreasing) scrolls content up (wheel +) when
    // natural. Horizontal: finger right scrolls content right (hwheel +) when natural.
    accumY_ += cfg_.naturalScroll ? -dCy : dCy;
    accumX_ += cfg_.naturalScroll ? dCx : -dCx;
    while (accumY_ >= ppn) {
        inj_.wheel(+kWheelDelta);
        accumY_ -= ppn;
    }
    while (accumY_ <= -ppn) {
        inj_.wheel(-kWheelDelta);
        accumY_ += ppn;
    }
    while (accumX_ >= ppn) {
        inj_.hwheel(+kWheelDelta);
        accumX_ -= ppn;
    }
    while (accumX_ <= -ppn) {
        inj_.hwheel(-kWheelDelta);
        accumX_ += ppn;
    }
}

void GestureSink::emitPinch(double dist) {
    const int ppn = std::max(1, cfg_.pinchPixelsPerNotch);
    accumPinch_ += dist - lastDist_;  // fingers apart -> positive -> zoom in
    lastDist_ = dist;
    while (accumPinch_ >= ppn) {
        ctrlWheel(+kWheelDelta);
        accumPinch_ -= ppn;
    }
    while (accumPinch_ <= -ppn) {
        ctrlWheel(-kWheelDelta);
        accumPinch_ += ppn;
    }
}

void GestureSink::ctrlWheel(int delta) {
    inj_.keyDown(vk::CTRL);
    inj_.wheel(delta);
    inj_.keyUp(vk::CTRL);
}

void GestureSink::handleThreeFinger(double cx, double cy) {
    if (committed_) {
        if (mode_ == Mode::AltTab) handleAltTab(cx);
        return;
    }
    const double dxs = cx - anchorCx_;
    const double dys = cy - anchorCy_;
    if (maxAxis(dxs, dys) <= cfg_.swipeCommitPx) return;

    committed_ = true;
    if (std::abs(dxs) >= std::abs(dys)) {
        // Horizontal: open interactive Alt+Tab and seed it with the swipe direction.
        mode_ = Mode::AltTab;
        altAnchorX_ = cx;
        enterAltTab(dxs > 0);
    } else if (dys < 0) {
        mode_ = Mode::Swipe3;
        inj_.keyCombo({vk::LWIN, vk::TAB});  // up: Task View
    } else {
        mode_ = Mode::Swipe3;
        inj_.keyCombo({vk::LWIN, vk::D});    // down: Show desktop
    }
}

void GestureSink::handleFour(double cx, double cy) {
    if (committed_) return;
    const double dxs = cx - anchorCx_;
    const double dys = cy - anchorCy_;
    if (maxAxis(dxs, dys) <= cfg_.swipeCommitPx) return;

    committed_ = true;
    mode_ = Mode::Swipe4;
    if (std::abs(dxs) >= std::abs(dys)) {
        inj_.keyCombo({vk::CTRL, vk::LWIN, dxs > 0 ? vk::LEFT : vk::RIGHT});
    } else if (cfg_.fourFingerVerticalMirror) {
        if (dys < 0)
            inj_.keyCombo({vk::LWIN, vk::TAB});
        else
            inj_.keyCombo({vk::LWIN, vk::D});
    }
}

void GestureSink::enterAltTab(bool forward) {
    inj_.keyDown(vk::ALT);
    altActive_ = true;
    tabStep(forward);  // first step reflects the commit direction (right=Tab, left=Shift+Tab)
}

void GestureSink::handleAltTab(double cx) {
    const int step = std::max(1, cfg_.altTabStepPx);
    while (cx - altAnchorX_ >= step) {
        tabStep(/*forward=*/true);
        altAnchorX_ += step;
    }
    while (cx - altAnchorX_ <= -step) {
        tabStep(/*forward=*/false);
        altAnchorX_ -= step;
    }
}

void GestureSink::tabStep(bool forward) {
    if (!forward) {
        inj_.keyDown(vk::SHIFT);
        shiftHeld_ = true;
    }
    inj_.keyDown(vk::TAB);
    inj_.keyUp(vk::TAB);
    if (!forward) {
        inj_.keyUp(vk::SHIFT);
        shiftHeld_ = false;
    }
}

void GestureSink::releaseAltTab() {
    if (!altActive_) return;
    if (shiftHeld_) {
        inj_.keyUp(vk::SHIFT);
        shiftHeld_ = false;
    }
    inj_.keyUp(vk::ALT);  // releasing Alt confirms the highlighted window
    altActive_ = false;
}

void GestureSink::finalizeOnLift(std::uint32_t tsUs) {
    if (mode_ == Mode::AltTab) {
        releaseAltTab();  // lift confirms the Alt+Tab selection
        return;
    }
    // A short, near-stationary 2-finger session that never committed is a 2-finger tap
    // -> right click. Judged by peakCount_ (not sessionFingers_) so a quick tap that
    // lifts inside the settle window is still recognized.
    if (!committed_ && peakCount_ == 2) {
        const std::uint32_t durUs = tsUs - startUs_;  // u32 wrap-safe
        if (durUs <= cfg_.tapMaxUs && maxTravelPx_ <= cfg_.tapMovePx) {
            inj_.rightClick();
        }
    }
}

void GestureSink::reset() {
    releaseAltTab();  // safety net: hard reset / disconnect must release Alt
    active_ = false;
    committed_ = false;
    mode_ = Mode::None;
    peakCount_ = 0;
    settled_ = false;
    sessionFingers_ = 0;
}

}  // namespace phone2pad::client
