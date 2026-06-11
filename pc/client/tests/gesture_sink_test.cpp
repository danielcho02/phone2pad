// GestureSink unit tests (docs/05 L2): 2-finger scroll / tap / pinch, 3-finger
// vertical swipes and interactive Alt+Tab, 4-finger desktop switch, plus the negative
// cases that matter most (docs/05 §3): a scroll must not right-click or swipe, a tap
// must not scroll, a swipe fires once, the peak-finger latch and settle window must not
// let a 3-finger gesture or landing skew misfire, and Alt is held until lift.
//
// Multi-finger sessions hold briefly past the settle window (default 90ms) before the
// gesture proper, so timestamps run to a few hundred ms.
#include <algorithm>
#include <vector>

#include "mock_injector.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;
using phone2pad::proto::Contact;
using phone2pad::proto::TouchFrame;

namespace {

// Win32 VK codes the sink emits (mirrors gesture_sink.cpp).
constexpr int VK_LWIN_ = 0x5B, VK_TAB_ = 0x09, VK_SHIFT_ = 0x10, VK_CTRL_ = 0x11,
              VK_ALT_ = 0x12, VK_D_ = 0x44, VK_LEFT_ = 0x25, VK_RIGHT_ = 0x27;

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

int countVk(const std::vector<std::string>& events, const std::string& prefix, int vk) {
    const std::string needle = prefix + std::to_string(vk);
    int n = 0;
    for (const auto& e : events) {
        if (e == needle) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("two-finger scroll up emits positive vertical wheel, nothing else") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),  // settle
        frame(150000, {ct(0, true, 200, 240), ct(1, true, 400, 240)}),
        frame(200000, {ct(0, true, 200, 180), ct(1, true, 400, 180)}),
        frame(250000, {ct(0, true, 200, 120), ct(1, true, 400, 120)}),
        frame(300000, {ct(0, false, 200, 120), ct(1, false, 400, 120)}),
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
        frame(100000, {ct(0, true, 100, 300), ct(1, true, 300, 300)}),  // settle
        frame(150000, {ct(0, true, 160, 300), ct(1, true, 360, 300)}),
        frame(200000, {ct(0, true, 220, 300), ct(1, true, 420, 300)}),
        frame(250000, {ct(0, true, 300, 300), ct(1, true, 500, 300)}),
        frame(300000, {ct(0, false, 300, 300), ct(1, false, 500, 300)}),
    });
    CHECK(inj.hwheelSum > 0);
    CHECK_EQ(inj.wheelSum, 0L);
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("two-finger tap (inside settle window) emits one right click, no wheel") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(20000, {ct(0, true, 201, 300), ct(1, true, 400, 301)}),
        frame(40000, {ct(0, false, 201, 300), ct(1, false, 400, 301)}),  // lift < settle
    });
    CHECK_EQ(inj.rightClicks(), 1);
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.clicks(), 0);
}

TEST_CASE("two-finger press held ~200ms then lift emits one right click") {
    MockInjector inj;
    GestureSink s(inj);  // tapMaxUs=300ms default
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),   // past settle, still held
        frame(200000, {ct(0, true, 201, 300), ct(1, true, 399, 300)}),   // ~200ms, near-stationary
        frame(220000, {ct(0, false, 201, 300), ct(1, false, 399, 300)}), // lift < 300ms
    });
    CHECK_EQ(inj.rightClicks(), 1);   // deliberate press still counts as a tap
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.clicks(), 0);
}

TEST_CASE("two-finger tap with slight micro-movement still right clicks") {
    MockInjector inj;
    GestureSink s(inj);  // tapMovePx=24, scrollCommitPx=12
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(40000, {ct(0, true, 206, 308), ct(1, true, 408, 306)}),  // centroid drift < 24px
        frame(80000, {ct(0, false, 206, 308), ct(1, false, 408, 306)}),
    });
    CHECK_EQ(inj.rightClicks(), 1);   // wobble under the tap tolerance is not a scroll
    CHECK_EQ(inj.wheelEvents(), 0);
}

TEST_CASE("two-finger tap with lift skew right clicks, never scrolls") {
    MockInjector inj;
    GestureSink s(inj);
    // Held past the 90ms settle window, then one finger lifts a frame before the other.
    // The single-contact frame's centroid jumps ~100px to one finger; without the
    // activeCount==2 guard that jump would commit a scroll (stray wheel + suppressed tap).
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),  // settle
        frame(150000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),  // still held, no commit
        frame(170000, {ct(0, true, 200, 300)}),                         // finger 1 lifts first
        frame(190000, {ct(0, false, 200, 300)}),                        // finger 0 lifts -> all up
    });
    CHECK_EQ(inj.rightClicks(), 1);   // skew did not suppress the right click
    CHECK_EQ(inj.wheelEvents(), 0);   // and the centroid jump emitted no stray scroll
}

TEST_CASE("long/large two-finger move is a scroll, not a tap") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),  // settle
        frame(150000, {ct(0, true, 200, 220), ct(1, true, 400, 220)}),
        frame(200000, {ct(0, true, 200, 150), ct(1, true, 400, 150)}),
        frame(250000, {ct(0, false, 200, 150), ct(1, false, 400, 150)}),
    });
    CHECK(inj.wheelEvents() > 0);
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("three-finger swipe up = Win+Tab (Task View), no Alt") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(150000, {ct(0, true, 200, 240), ct(1, true, 300, 240), ct(2, true, 400, 240)}),
        frame(200000, {ct(0, true, 200, 180), ct(1, true, 300, 180), ct(2, true, 400, 180)}),
        frame(250000, {ct(0, false, 200, 180), ct(1, false, 300, 180), ct(2, false, 400, 180)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.rightClicks(), 0);
    CHECK_EQ(countVk(inj.events, "key_down ", VK_ALT_), 0);
}

TEST_CASE("three-finger swipe down = Win+D (Show desktop)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 150), ct(1, true, 300, 150), ct(2, true, 400, 150)}),
        frame(100000, {ct(0, true, 200, 150), ct(1, true, 300, 150), ct(2, true, 400, 150)}),
        frame(150000, {ct(0, true, 200, 220), ct(1, true, 300, 220), ct(2, true, 400, 220)}),
        frame(200000, {ct(0, true, 200, 280), ct(1, true, 300, 280), ct(2, true, 400, 280)}),
        frame(250000, {ct(0, false, 200, 280), ct(1, false, 300, 280), ct(2, false, 400, 280)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_D_}));
}

TEST_CASE("three-finger right opens Alt+Tab; Alt held until lift, released on lift") {
    MockInjector inj;
    GestureSink s(inj);
    // Hold through settle, then move right far enough to commit + step.
    const std::vector<TouchFrame> touching = {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(150000, {ct(0, true, 290, 300), ct(1, true, 390, 300), ct(2, true, 490, 300)}),  // commit
        frame(200000, {ct(0, true, 380, 300), ct(1, true, 480, 300), ct(2, true, 580, 300)}),  // step
    };
    feed(s, touching);
    // Before lift: Alt is down, Tab pressed at least once, Alt NOT yet released.
    CHECK_EQ(countVk(inj.events, "key_down ", VK_ALT_), 1);
    CHECK(countVk(inj.events, "key_down ", VK_TAB_) >= 1);
    CHECK_EQ(countVk(inj.events, "key_up ", VK_ALT_), 0);
    // Shift never used on a rightward swipe.
    CHECK_EQ(countVk(inj.events, "key_down ", VK_SHIFT_), 0);
    // No other gesture mixed in.
    CHECK_EQ(inj.wheelEvents(), 0);
    CHECK_EQ(inj.rightClicks(), 0);

    // Lift (== ACTION_CANCEL == disconnect emitLift path) confirms: Alt released once.
    s.onFrame(frame(250000, {ct(0, false, 380, 300), ct(1, false, 480, 300), ct(2, false, 580, 300)}));
    CHECK_EQ(countVk(inj.events, "key_up ", VK_ALT_), 1);
}

TEST_CASE("three-finger left uses Shift+Tab; Shift balanced; Alt released on lift") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 400, 300), ct(1, true, 500, 300), ct(2, true, 600, 300)}),
        frame(100000, {ct(0, true, 400, 300), ct(1, true, 500, 300), ct(2, true, 600, 300)}),
        frame(150000, {ct(0, true, 310, 300), ct(1, true, 410, 300), ct(2, true, 510, 300)}),  // commit left
        frame(200000, {ct(0, true, 230, 300), ct(1, true, 330, 300), ct(2, true, 430, 300)}),  // step left
        frame(250000, {ct(0, false, 230, 300), ct(1, false, 330, 300), ct(2, false, 430, 300)}),
    });
    CHECK(countVk(inj.events, "key_down ", VK_SHIFT_) >= 1);  // left = Shift+Tab
    CHECK(countVk(inj.events, "key_down ", VK_TAB_) >= 1);
    // Shift fully balanced (never left held), Alt released exactly once at lift.
    CHECK_EQ(countVk(inj.events, "key_down ", VK_SHIFT_),
             countVk(inj.events, "key_up ", VK_SHIFT_));
    CHECK_EQ(countVk(inj.events, "key_up ", VK_ALT_), 1);
    CHECK_EQ(inj.wheelEvents(), 0);
}

TEST_CASE("four-finger swipe right = Ctrl+Win+Right (next virtual desktop)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 100, 300), ct(1, true, 150, 300), ct(2, true, 200, 300), ct(3, true, 250, 300)}),
        frame(100000, {ct(0, true, 100, 300), ct(1, true, 150, 300), ct(2, true, 200, 300), ct(3, true, 250, 300)}),
        frame(150000, {ct(0, true, 170, 300), ct(1, true, 220, 300), ct(2, true, 270, 300), ct(3, true, 320, 300)}),
        frame(200000, {ct(0, true, 220, 300), ct(1, true, 270, 300), ct(2, true, 320, 300), ct(3, true, 370, 300)}),
        frame(250000, {ct(0, false, 220, 300), ct(1, false, 270, 300), ct(2, false, 320, 300), ct(3, false, 370, 300)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_CTRL_, VK_LWIN_, VK_RIGHT_}));
}

TEST_CASE("swipe fires exactly once per session (no re-trigger)") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(150000, {ct(0, true, 200, 200), ct(1, true, 300, 200), ct(2, true, 400, 200)}),  // commit up
        frame(200000, {ct(0, true, 200, 120), ct(1, true, 300, 120), ct(2, true, 400, 120)}),  // keeps moving
        frame(250000, {ct(0, true, 200, 60), ct(1, true, 300, 60), ct(2, true, 400, 60)}),
        frame(300000, {ct(0, false, 200, 60), ct(1, false, 300, 60), ct(2, false, 400, 60)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));  // not doubled
}

TEST_CASE("peak-finger latch: 3-finger gesture that dips to 2 never scrolls") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 300, 300), ct(2, true, 400, 300)}),  // settle -> 3
        frame(150000, {ct(0, true, 200, 250), ct(2, true, 400, 250)}),  // one finger momentarily gone
        frame(200000, {ct(0, true, 200, 180), ct(1, true, 300, 180), ct(2, true, 400, 180)}),  // commit up
        frame(250000, {ct(0, false, 200, 180), ct(1, false, 300, 180), ct(2, false, 400, 180)}),
    });
    CHECK_EQ(inj.wheelEvents(), 0);   // never demoted to a 2-finger scroll
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_LWIN_, VK_TAB_}));
}

TEST_CASE("two-finger pinch out = Ctrl+wheel zoom in") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),
        frame(100000, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),  // settle
        frame(150000, {ct(0, true, 270, 300), ct(1, true, 370, 300)}),
        frame(200000, {ct(0, true, 230, 300), ct(1, true, 410, 300)}),
        frame(250000, {ct(0, true, 200, 300), ct(1, true, 440, 300)}),
        frame(300000, {ct(0, false, 200, 300), ct(1, false, 440, 300)}),
    });
    CHECK(inj.wheelSum > 0);                   // apart -> zoom in
    CHECK(!inj.keyDowns.empty());
    CHECK_EQ(inj.keyDowns.front(), VK_CTRL_);  // each notch is Ctrl+wheel
    CHECK_EQ(inj.rightClicks(), 0);
}

TEST_CASE("pinch disabled: same motion emits no Ctrl+wheel") {
    GestureConfig cfg;
    cfg.enablePinch = false;
    MockInjector inj;
    GestureSink s(inj, cfg);
    feed(s, {
        frame(0, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),
        frame(100000, {ct(0, true, 300, 300), ct(1, true, 340, 300)}),
        frame(150000, {ct(0, true, 270, 300), ct(1, true, 370, 300)}),
        frame(200000, {ct(0, true, 230, 300), ct(1, true, 410, 300)}),
        frame(250000, {ct(0, true, 200, 300), ct(1, true, 440, 300)}),
        frame(300000, {ct(0, false, 200, 300), ct(1, false, 440, 300)}),
    });
    CHECK(inj.keyDowns.empty());  // no Ctrl held -> not a pinch
}

TEST_CASE("settle window: 3-then-4 landing skew is a 4-finger desktop switch") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        // Only 3 fingers land first; the 4th arrives 50ms later (inside settle window).
        frame(0, {ct(0, true, 400, 300), ct(1, true, 450, 300), ct(2, true, 500, 300)}),
        frame(50000, {ct(0, true, 400, 300), ct(1, true, 450, 300), ct(2, true, 500, 300), ct(3, true, 550, 300)}),
        frame(100000, {ct(0, true, 400, 300), ct(1, true, 450, 300), ct(2, true, 500, 300), ct(3, true, 550, 300)}),
        frame(150000, {ct(0, true, 360, 300), ct(1, true, 410, 300), ct(2, true, 460, 300), ct(3, true, 510, 300)}),
        frame(200000, {ct(0, true, 310, 300), ct(1, true, 360, 300), ct(2, true, 410, 300), ct(3, true, 460, 300)}),  // commit left
        frame(250000, {ct(0, false, 310, 300), ct(1, false, 360, 300), ct(2, false, 410, 300), ct(3, false, 460, 300)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_CTRL_, VK_LWIN_, VK_LEFT_}));  // 4-finger
    // Negative: no 3-finger gesture misfired (no Alt held, no Win+Tab opener).
    CHECK_EQ(countVk(inj.events, "key_down ", VK_ALT_), 0);
}

TEST_CASE("4->3 decay tail after a four-finger commit fires no 3-finger gesture") {
    MockInjector inj;
    GestureSink s(inj);
    feed(s, {
        frame(0, {ct(0, true, 100, 300), ct(1, true, 150, 300), ct(2, true, 200, 300), ct(3, true, 250, 300)}),
        frame(50000, {ct(0, true, 100, 300), ct(1, true, 150, 300), ct(2, true, 200, 300), ct(3, true, 250, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 250, 300), ct(2, true, 300, 300), ct(3, true, 350, 300)}),  // commit right
        // Now one finger lifts (decay to 3) and the hand keeps moving.
        frame(150000, {ct(0, true, 280, 300), ct(1, true, 330, 300), ct(2, true, 380, 300)}),
        frame(200000, {ct(0, true, 360, 300), ct(1, true, 410, 300), ct(2, true, 460, 300)}),
        frame(250000, {ct(0, false, 360, 300), ct(1, false, 410, 300), ct(2, false, 460, 300)}),
    });
    CHECK_EQ(inj.keyDowns, (std::vector<int>{VK_CTRL_, VK_LWIN_, VK_RIGHT_}));  // exactly one switch
    CHECK_EQ(countVk(inj.events, "key_down ", VK_ALT_), 0);  // no 3-finger Alt+Tab
}
