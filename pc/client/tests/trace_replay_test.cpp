// L3 (docs/05 §4): replay the synthetic .pptrace fixtures through MouseSink with a
// MockInjector — exercises the full PC pipeline (trace -> replay -> sink) phone-free.
#include <string>

#include "mock_injector.hpp"
#include "phone2pad/client/gesture_router.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/replay.hpp"
#include "phone2pad/client/trace.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;

namespace {

std::string fixture(const char* name) {
    return std::string(PHONE2PAD_FIXTURE_DIR) + "/" + name;
}

// Replay a fixture through the full GestureRouter (1-finger -> mouse, 2+ -> gesture),
// the same wiring the real client uses. Returns the gesture sink's recorded events.
MockInjector replayGesture(const char* name) {
    const std::vector<TimedFrame> frames = readTrace(fixture(name));
    MockInjector mouseInj, gestInj;
    MouseSink mouse(mouseInj);
    GestureSink gesture(gestInj);
    GestureRouter router(mouse, gesture);
    replayFrames(frames, router, 0.0, nullptr);
    return gestInj;
}

}  // namespace

TEST_CASE("replay one_finger_move: cursor moves, no click") {
    const std::vector<TimedFrame> frames = readTrace(fixture("one_finger_move.pptrace"));
    CHECK(frames.size() == 4);
    MockInjector inj;
    MouseSink sink(inj);
    replayFrames(frames, sink, 0.0, nullptr);  // speed 0 -> no real sleeping
    CHECK(inj.moves >= 1);
    CHECK_EQ(inj.clicks(), 0);
}

TEST_CASE("replay one_finger_tap: two clicks, no move") {
    const std::vector<TimedFrame> frames = readTrace(fixture("one_finger_tap.pptrace"));
    MockInjector inj;
    MouseSink sink(inj);
    replayFrames(frames, sink, 0.0, nullptr);
    CHECK_EQ(inj.clicks(), 2);
    CHECK_EQ(inj.moves, 0);
}

TEST_CASE("replay two_finger_scroll_v: vertical wheel, no swipe/right-click") {
    const MockInjector g = replayGesture("two_finger_scroll_v.pptrace");
    CHECK(g.wheelSum > 0);
    CHECK_EQ(g.hwheelSum, 0L);
    CHECK_EQ(g.rightClicks(), 0);
    CHECK(g.keyDowns.empty());
}

TEST_CASE("replay two_finger_scroll_h: horizontal wheel only") {
    const MockInjector g = replayGesture("two_finger_scroll_h.pptrace");
    CHECK(g.hwheelSum > 0);
    CHECK_EQ(g.wheelSum, 0L);
}

TEST_CASE("replay two_finger_tap: one right click, no wheel") {
    const MockInjector g = replayGesture("two_finger_tap.pptrace");
    CHECK_EQ(g.rightClicks(), 1);
    CHECK_EQ(g.wheelEvents(), 0);
}

TEST_CASE("replay three_finger_swipe_up: Win+Tab, no scroll") {
    const MockInjector g = replayGesture("three_finger_swipe_up.pptrace");
    CHECK_EQ(g.keyDowns, (std::vector<int>{0x5B, 0x09}));
    CHECK_EQ(g.wheelEvents(), 0);
}

TEST_CASE("replay four_finger_swipe_left: Ctrl+Win+Right despite 3->4 landing skew") {
    const MockInjector g = replayGesture("four_finger_swipe_left.pptrace");
    CHECK_EQ(g.keyDowns, (std::vector<int>{0x11, 0x5B, 0x27}));  // left swipe -> right (next) desktop
    CHECK(g.keyDowns.size() == 3);  // no extra 3-finger Alt(0x12) misfire
}

TEST_CASE("replay three_finger_alt_tab_right: Alt held, Tab steps, Alt up on lift") {
    const MockInjector g = replayGesture("three_finger_alt_tab_right.pptrace");
    int altDown = 0, altUp = 0, tabDown = 0;
    for (const auto& e : g.events) {
        if (e == "key_down 18") ++altDown;  // 0x12 VK_MENU
        else if (e == "key_up 18") ++altUp;
        else if (e == "key_down 9") ++tabDown;  // 0x09 VK_TAB
    }
    CHECK_EQ(altDown, 1);
    CHECK_EQ(altUp, 1);          // released exactly once, on lift
    CHECK(tabDown >= 2);         // opener + at least one step
    CHECK_EQ(g.wheelEvents(), 0);
    CHECK_EQ(g.rightClicks(), 0);
}

TEST_CASE("replay pinch_zoom: Ctrl+wheel zoom in, no right-click") {
    const MockInjector g = replayGesture("pinch_zoom.pptrace");
    CHECK(g.wheelSum > 0);
    CHECK(!g.keyDowns.empty());
    CHECK_EQ(g.keyDowns.front(), 0x11);
    CHECK_EQ(g.rightClicks(), 0);
}
