// Real input injection via Win32 SendInput.
// https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput
#pragma once

#include "phone2pad/client/input_injector.hpp"

namespace phone2pad::client {

class Win32InputInjector : public InputInjector {
public:
    void moveRelative(int dx, int dy) override;
    void leftDown() override;
    void leftUp() override;
    void rightDown() override;
    void rightUp() override;
    void wheel(int delta) override;
    void hwheel(int delta) override;
    void keyDown(int vk) override;
    void keyUp(int vk) override;
};

}  // namespace phone2pad::client
