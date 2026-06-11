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

    void moveRelative(int dx, int dy) override {
        events.push_back("move " + std::to_string(dx) + " " + std::to_string(dy));
        ++moves;
        sumDx += dx;
        sumDy += dy;
    }
    void leftDown() override { events.push_back("left_down"); }
    void leftUp() override { events.push_back("left_up"); }

    int clicks() const {
        int n = 0;
        for (const auto& e : events) {
            if (e == "left_down") ++n;
        }
        return n;
    }
};

}  // namespace phone2pad::client
