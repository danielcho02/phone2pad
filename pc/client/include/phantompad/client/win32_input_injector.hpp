// Real input injection via Win32 SendInput.
// https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput
#pragma once

#include "phantompad/client/input_injector.hpp"

namespace phantompad::client {

class Win32InputInjector : public InputInjector {
public:
    void moveRelative(int dx, int dy) override;
    void leftDown() override;
    void leftUp() override;
};

}  // namespace phantompad::client
