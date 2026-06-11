// Abstraction over OS input injection so MouseSink is testable without SendInput.
// Win32InputInjector is the real implementation; MockInputInjector (tests) records
// the event sequence.
#pragma once

namespace phone2pad::client {

struct InputInjector {
    virtual ~InputInjector() = default;
    // Relative mouse move (MOUSEEVENTF_MOVE semantics).
    virtual void moveRelative(int dx, int dy) = 0;
    virtual void leftDown() = 0;
    virtual void leftUp() = 0;

    void leftClick() {
        leftDown();
        leftUp();
    }
};

}  // namespace phone2pad::client
