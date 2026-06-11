// Abstraction over OS input injection so the sinks are testable without SendInput.
// Win32InputInjector is the real implementation; MockInjector (tests) and
// LoggingInjector (replay) record/print the event sequence instead.
#pragma once

#include <initializer_list>
#include <iterator>

namespace phone2pad::client {

struct InputInjector {
    virtual ~InputInjector() = default;
    // Relative mouse move (MOUSEEVENTF_MOVE semantics).
    virtual void moveRelative(int dx, int dy) = 0;
    virtual void leftDown() = 0;
    virtual void leftUp() = 0;
    virtual void rightDown() = 0;
    virtual void rightUp() = 0;
    // Wheel scroll. `delta` is in WHEEL_DELTA (120) units; positive = up/right per
    // Win32 MOUSEEVENTF_WHEEL/_HWHEEL conventions (GestureSink maps direction).
    virtual void wheel(int delta) = 0;
    virtual void hwheel(int delta) = 0;
    // Virtual-key press/release using Win32 VK_* codes.
    virtual void keyDown(int vk) = 0;
    virtual void keyUp(int vk) = 0;

    void leftClick() {
        leftDown();
        leftUp();
    }
    void rightClick() {
        rightDown();
        rightUp();
    }
    // Chord: press keys in order, release in reverse (e.g. {VK_LWIN, VK_TAB}).
    void keyCombo(std::initializer_list<int> vks) {
        for (int vk : vks) keyDown(vk);
        for (auto it = std::rbegin(vks); it != std::rend(vks); ++it) keyUp(*it);
    }
};

}  // namespace phone2pad::client
