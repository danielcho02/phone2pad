// Test double for InputInjector: records the injected event sequence.
#pragma once

#include <string>
#include <vector>

#include "phone2pad/client/input_injector.hpp"

namespace phone2pad::client {

struct MockInjector : InputInjector {
    std::vector<std::string> events;
    int moves = 0;
    long sumDx = 0;
    long sumDy = 0;
    long wheelSum = 0;          // net vertical wheel delta (WHEEL_DELTA units)
    long hwheelSum = 0;         // net horizontal wheel delta
    std::vector<int> keyDowns;  // VK codes in press order (for chord assertions)

    static std::string withSign(int v) {
        return (v >= 0 ? "+" : "") + std::to_string(v);
    }

    void moveRelative(int dx, int dy) override {
        events.push_back("move " + std::to_string(dx) + " " + std::to_string(dy));
        ++moves;
        sumDx += dx;
        sumDy += dy;
    }
    void leftDown() override { events.push_back("left_down"); }
    void leftUp() override { events.push_back("left_up"); }
    void rightDown() override { events.push_back("right_down"); }
    void rightUp() override { events.push_back("right_up"); }
    void wheel(int delta) override {
        events.push_back("wheel " + withSign(delta));
        wheelSum += delta;
    }
    void hwheel(int delta) override {
        events.push_back("hwheel " + withSign(delta));
        hwheelSum += delta;
    }
    void keyDown(int vk) override {
        events.push_back("key_down " + std::to_string(vk));
        keyDowns.push_back(vk);
    }
    void keyUp(int vk) override { events.push_back("key_up " + std::to_string(vk)); }

    int count(const char* name) const {
        int n = 0;
        for (const auto& e : events) {
            if (e == name) ++n;
        }
        return n;
    }
    int clicks() const { return count("left_down"); }
    int rightClicks() const { return count("right_down"); }
    int wheelEvents() const {
        int n = 0;
        for (const auto& e : events) {
            if (e.rfind("wheel ", 0) == 0 || e.rfind("hwheel ", 0) == 0) ++n;
        }
        return n;
    }
};

}  // namespace phone2pad::client
