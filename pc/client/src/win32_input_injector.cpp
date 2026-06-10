#include "phantompad/client/win32_input_injector.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace phantompad::client {

namespace {

// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-input
// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-mouseinput
void sendMouse(DWORD flags, LONG dx, LONG dy) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.dwFlags = flags;
    SendInput(1, &in, sizeof(INPUT));
}

}  // namespace

void Win32InputInjector::moveRelative(int dx, int dy) {
    // MOUSEEVENTF_MOVE is relative (no MOUSEEVENTF_ABSOLUTE); OS pointer
    // speed/acceleration applies.
    sendMouse(MOUSEEVENTF_MOVE, static_cast<LONG>(dx), static_cast<LONG>(dy));
}

void Win32InputInjector::leftDown() {
    sendMouse(MOUSEEVENTF_LEFTDOWN, 0, 0);
}

void Win32InputInjector::leftUp() {
    sendMouse(MOUSEEVENTF_LEFTUP, 0, 0);
}

}  // namespace phantompad::client

#endif  // _WIN32
