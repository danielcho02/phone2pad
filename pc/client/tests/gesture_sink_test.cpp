// GestureSink unit tests (docs/05 L2): 2-finger scroll / tap / pinch, 3- and
// 4-finger swipes, plus the negative cases that matter most (docs/05 §3): a scroll
// must not right-click or fire a swipe, a tap must not scroll, a swipe fires once,
// and the peak-finger latch must not let a 3-finger gesture decay into a scroll.
#include <vector>

#include "mock_injector.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;
using phone2pad::proto::Contact;
using phone2pad::proto::TouchFrame;

namespace {

// Win32 VK codes the sink emits (mirrors gesture_sink.cpp).
constexpr int VK_LWIN_ = 0x5B, VK_TAB_ = 0x09, VK_ALT_ = 0x12, VK_SHIFT_ = 0x10,
              VK_CTRL_ = 0x11, VK_D_ = 0x44, VK_LEFT_ = 0x25, VK_RIGHT_ = 0x27;

Contact ct(std::uint8_t id, bool tip, std::uint16_t x, std::uint16_t y) {
    Contact c;
    c.id = id;
    c.tip = tip;
    c.confidence = true;
    c.x = x;
    c.y = y;
    return c;
}

TouchFrame frame(std::uint32_t t, std::vector<Contact> contacts) {
    TouchFrame f;
    f.timestampUs = t;
    f.contacts = std::move(contacts);
    return f;
}

void feed(GestureSink& s, const std::vector<TouchFrame>& frames) {
    for (const auto& f : frames) s.onFrame(f);
}

}  // namespace

TEST_CASE("two-finger scroll up emits positive vertical wheel, nothing else") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(8333, {ct(0, true, 200, 250), ct(1, true, 400, 250)}),
        frame(16666, {ct(0, true, 200, 200), ct(1, true, 400, 200)}),
        frame(25000, {ct(0, true, 200, 150), ct(1, true, 400, 150)}),
        frame(33333, {ct(0, false, 200, 150), ct(1, false, 400, 150)}),
    });
    CHECK(inj.wheelSum > 0);          // natural: finger up -> content up
    CHECK_EQ(inj.hwheelSum, 0L);
    CHECK_EQ(inj.rightClicks(), 0);   // a scroll is never a tap
    CHECK(inj.keyDowns.empty());      // a 2-finger scroll fires no swipe
}

TEST_CASE("two-finger scroll right emits horizontal wheel only") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 100, 300), ct(1, true, 300, 300)}),
        frame(8333, {ct(0, true, 160, 300), ct(1, true, 360, 300)}),
        frame(16666, {ct(0, true, 220, 300), ct(1, true, 420, 300)}),
        frame(25000, {ct(0, true, 300, 300), ct(1, true, 500, 300)}),
        frame(33333, {ct(0, false, 300, 300), ct(1, false, 500, 300)}),
    });
    CHECK(inj.hwheelSum > 0);
    CHECK_EQ(inj.wheelSum, 0L);
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("two-finger tap emits one right click, no wheel") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(20000, {ct(0, true, 201, 300), ct(1, true, 400, 301)}),
        frame(40000, {ct(0, false, 201, 300), ct(1, false, 400, 301)}),
    });
    CHECK_EQ(inj.rightClicks(), 1);
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.clicks(), 0);
}

TEST_CASE("long/large two-finger move is a scroll, not a tap") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(8333, {ct(0, true, 200, 220), ct(1, true, 400, 220)}),
        frame(16666, {ct(0, true, 200, 150), ct(1, true, 400, 150)}),
        frame(25000, {ct(0, false, 200, 150), ct(1, false, 400, 150)}),
    });
    CHECK(inj.wheelEvents() > 0);
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("three-finger swipe up = Win+Tab (Task View)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(8333, {ct(0, true, 200, 240), ct(1, true, 300, 240), ct(2, true, 400, 240)}),
        frame(16666, {ct(0, true, 200, 180), ct(1, true, 300, 180), ct(2, true, 400, 180)}),
        frame(25000, {ct(0, false, 200, 180), ct(1, false, 300, 180), ct(2, false, 400, 180)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("three-finger swipe down = Win+D (Show desktop)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 150), ct(1, true, 300, 150), ct(2, true, 400, 150)}),
        frame(8333, {ct(0, true, 200, 220), ct(1, true, 300, 220), ct(2, true, 400, 220)}),
        frame(16666, {ct(0, true, 200, 280), ct(1, true, 300, 280), ct(2, true, 400, 280)}),
        frame(25000, {ct(0, false, 200, 280), ct(1, false, 300, 280), ct(2, false, 400, 280)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_D_}));
}

TEST_CASE("three-finger swipe right = Alt+Tab, left = Alt+Shift+Tab") {
    MockInjector right, left;
    GestureSink sr(right), sl(left);
    feed(sr, {
        frame(0, {ct(0, true, 100, 300), ct(1, true, 200, 300), ct(2, true, 300, 300)}),
        frame(8333, {ct(0, true, 170, 300), ct(1, true, 270, 300), ct(2, true, 370, 300)}),
        frame(16666, {ct(0, true, 220, 300), ct(1, true, 320, 300), ct(2, true, 420, 300)}),
        frame(25000, {ct(0, false, 220, 300), ct(1, false, 320, 300), ct(2, false, 420, 300)}),
    });
    CHECK_EQ(right.keyDowns, (std::vector<int>{VK_ALT_, VK_TAB_}));
    feed(sl, {
        frame(0, {ct(0, true, 400, 300), ct(1, true, 500, 300), ct(2, true, 600, 300)}),
        frame(8333, {ct(0, true, 330, 300), ct(1, true, 430, 300), ct(2, true, 530, 300)}),
        frame(16666, {ct(0, true, 280, 300), ct(1, true, 380, 300), ct(2, true, 480, 300)}),
        frame(25000, {ct(0, false, 280, 300), ct(1, false, 380, 300), ct(2, false, 480, 300)}),
    });
    CHECK_EQ(left.keyDowns, (std::vector<int>{VK_ALT_, VK_SHIFT_, VK_TAB_}));
}

TEST_CASE("four-finger swipe right = Ctrl+Win+Right (next virtual desktop)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 100, 300), ct(1, true, 150, 300), ct(2, true, 200, 300), ct(3, true, 250, 300)}),
        frame(8333, {ct(0, true, 170, 300), ct(1, true, 220, 300), ct(2, true, 270, 300), ct(3, true, 320, 300)}),
        frame(16666, {ct(0, true, 220, 300), ct(1, true, 270, 300), ct(2, true, 320, 300), ct(3, true, 370, 300)}),
        frame(25000, {ct(0, false, 220, 300), ct(1, false, 270, 300), ct(2, false, 320, 300), ct(3, false, 370, 300)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_CTRL_, VK_LWIN_, VK_RIGHT_}));
}

TEST_CASE("swipe fires exactly once per session (no re-trigger)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(8333, {ct(0, true, 200, 200), ct(1, true, 300, 200), ct(2, true, 400, 200)}),
        frame(16666, {ct(0, true, 200, 120), ct(1, true, 300, 120), ct(2, true, 400, 120)}),  // keeps moving
        frame(25000, {ct(0, true, 200, 60), ct(1, true, 300, 60), ct(2, true, 400, 60)}),
        frame(33333, {ct(0, false, 200, 60), ct(1, false, 300, 60), ct(2, false, 400, 60)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));  // not doubled
}

TEST_CASE("peak-finger latch: 3-finger gesture that dips to 2 never scrolls") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        // One finger momentarily reads as lifted while the hand moves up.
        frame(8333, {ct(0, true, 200, 250), ct(1, true, 300, 250)}),
        frame(16666, {ct(0, true, 200, 200), ct(1, true, 300, 200), ct(2, true, 400, 200)}),
        frame(25000, {ct(0, false, 200, 200), ct(1, false, 300, 200), ct(2, false, 400, 200)}),
    });
    CHECK_EQ(inj.wheelEvents(), 0);   // never demoted to a 2-finger scroll
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));
}

TEST_CASE("two-finger pinch out = Ctrl+wheel zoom in") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),
        frame(8333, {ct(0, true, 270, 300), ct(1, true, 370, 300)}),
        frame(16666, {ct(0, true, 230, 300), ct(1, true, 410, 300)}),
        frame(25000, {ct(0, true, 200, 300), ct(1, true, 440, 300)}),
        frame(33333, {ct(0, false, 200, 300), ct(1, false, 440, 300)}),
    });
    CHECK(inj.wheelSum > 0);                  // apart -> zoom in
    CHECK(!inj.keyDowns.empty());
    CHECK_EQ(inj.keyDowns.front(), VK_CTRL_); // each notch is Ctrl+wheel
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("pinch disabled: same motion emits no Ctrl+wheel") {
    GestureConfig cfg;
    cfg.enablePinch = false;
    MockInjector inj;
    GestureSink s(inj, cfg);
    feed(s, {
        frame(0, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),
        frame(8333, {ct(0, true, 270, 300), ct(1, true, 370, 300)}),
        frame(16666, {ct(0, true, 230, 300), ct(1, true, 410, 300)}),
        frame(25000, {ct(0, true, 200, 300), ct(1, true, 440, 300)}),
        frame(33333, {ct(0, false, 200, 300), ct(1, false, 440, 300)}),
    });
    CHECK(inj.keyDowns.empty());  // no Ctrl held -> not a pinch
}
