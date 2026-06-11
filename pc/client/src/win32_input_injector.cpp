#include "phone2pad/client/win32_input_injector.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace phone2pad::client {

namespace {

// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-input
// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-mouseinput
// mouseData carries the wheel delta for MOUSEEVENTF_WHEEL/_HWHEEL (signed,
// multiples of WHEEL_DELTA=120).
void sendMouse(DWORD flags, LONG dx, LONG dy, DWORD mouseData = 0) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.mouseData = mouseData;
    in.mi.dwFlags = flags;
    SendInput(1, &in, sizeof(INPUT));
}

// Navigation / modifier keys in the extended-key set; SendInput flags them so apps
// see the correct (E0-prefixed) key.
// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-keybdinput
bool isExtendedKey(int vk) {
    switch (vk) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_LWIN:
        case VK_RWIN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
            return true;
        default:
            return false;
    }
}

// https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-keybdinput
void sendKey(int vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = static_cast<WORD>(vk);
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    if (isExtendedKey(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
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

void Win32InputInjector::rightDown() {
    sendMouse(MOUSEEVENTF_RIGHTDOWN, 0, 0);
}

void Win32InputInjector::rightUp() {
    sendMouse(MOUSEEVENTF_RIGHTUP, 0, 0);
}

void Win32InputInjector::wheel(int delta) {
    sendMouse(MOUSEEVENTF_WHEEL, 0, 0, static_cast<DWORD>(delta));
}

void Win32InputInjector::hwheel(int delta) {
    sendMouse(MOUSEEVENTF_HWHEEL, 0, 0, static_cast<DWORD>(delta));
}

void Win32InputInjector::keyDown(int vk) {
    sendKey(vk, /*up=*/false);
}

void Win32InputInjector::keyUp(int vk) {
    sendKey(vk, /*up=*/true);
}

}  // namespace phone2pad::client

#endif  // _WIN32
