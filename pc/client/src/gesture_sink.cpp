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
    maxTravelPx_ = std::max(maxTravelPx_, maxAxis(a.cx - anchorCx_, a.cy - anchorCy_));

    if (peakCount_ >= 4) {
        handleSwipe(a.cx, a.cy, 4);
    } else if (peakCount_ == 3) {
        handleSwipe(a.cx, a.cy, 3);
    } else {
        handleTwo(a.cx, a.cy, a.dist, a.count);
    }
}

void GestureSink::begin(double cx, double cy, double dist, int count, std::uint32_t tsUs) {
    active_ = true;
    peakCount_ = count;
    startUs_ = tsUs;
    anchorCx_ = lastCx_ = cx;
    anchorCy_ = lastCy_ = cy;
    anchorDist_ = lastDist_ = dist;
    accumX_ = accumY_ = accumPinch_ = 0.0;
    maxTravelPx_ = 0.0;
    committed_ = false;
    mode_ = Mode::None;
}

void GestureSink::handleTwo(double cx, double cy, double dist, int activeCount) {
    if (!committed_) {
        const double move = maxAxis(cx - anchorCx_, cy - anchorCy_);
        const double dChg = (activeCount == 2) ? std::abs(dist - anchorDist_) : 0.0;
        const bool pinchOk = cfg_.enablePinch;
        if (move > cfg_.tapMovePx || (pinchOk && dChg > cfg_.tapMovePx)) {
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
    } else if (mode_ == Mode::Pinch && activeCount == 2) {
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

void GestureSink::handleSwipe(double cx, double cy, int fingers) {
    if (committed_) return;
    const double dxs = cx - anchorCx_;
    const double dys = cy - anchorCy_;
    if (maxAxis(dxs, dys) <= cfg_.swipeCommitPx) return;

    const bool horizontal = std::abs(dxs) >= std::abs(dys);
    committed_ = true;
    mode_ = (fingers == 4) ? Mode::Swipe4 : Mode::Swipe3;

    if (fingers == 3) {
        if (horizontal) {
            if (dxs > 0)
                inj_.keyCombo({vk::ALT, vk::TAB});           // right: next app
            else
                inj_.keyCombo({vk::ALT, vk::SHIFT, vk::TAB});  // left: previous app
        } else if (dys < 0) {
            inj_.keyCombo({vk::LWIN, vk::TAB});  // up: Task View
        } else {
            inj_.keyCombo({vk::LWIN, vk::D});    // down: Show desktop
        }
        return;
    }

    // 4 fingers: left/right switch virtual desktops; up/down optionally mirror 3-finger.
    if (horizontal) {
        inj_.keyCombo({vk::CTRL, vk::LWIN, dxs > 0 ? vk::RIGHT : vk::LEFT});
    } else if (cfg_.fourFingerVerticalMirror) {
        if (dys < 0)
            inj_.keyCombo({vk::LWIN, vk::TAB});
        else
            inj_.keyCombo({vk::LWIN, vk::D});
    }
}

void GestureSink::finalizeOnLift(std::uint32_t tsUs) {
    // A short, near-stationary 2-finger session that never committed to scroll/pinch
    // is a 2-finger tap -> right click.
    if (peakCount_ == 2 && !committed_) {
        const std::uint32_t durUs = tsUs - startUs_;  // u32 wrap-safe
        if (durUs <= cfg_.tapMaxUs && maxTravelPx_ <= cfg_.tapMovePx) {
            inj_.rightClick();
        }
    }
}

void GestureSink::reset() {
    active_ = false;
    committed_ = false;
    mode_ = Mode::None;
    peakCount_ = 0;
}

}  // namespace phone2pad::client
