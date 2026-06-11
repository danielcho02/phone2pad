// MouseSink unit tests (docs/05 L2): single-finger move, tap-to-click, tap
// rejection, and the multi-touch freeze + anchor-reset policy.
#include "mock_injector.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;
using phone2pad::proto::Contact;
using phone2pad::proto::TouchFrame;

namespace {

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

}  // namespace

TEST_CASE("single finger move emits relative deltas, no click") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 100, 100)}));       // anchor only
    s.onFrame(frame(1000, {ct(0, true, 110, 108)}));    // +10,+8
    s.onFrame(frame(2000, {ct(0, true, 110, 108)}));    // no movement -> no event
    CHECK_EQ(inj.moves, 1);
    CHECK_EQ(inj.sumDx, 10L);
    CHECK_EQ(inj.sumDy, 8L);
    CHECK_EQ(inj.clicks(), 0);
}

TEST_CASE("short tap in place emits one left click") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 200, 200)}));
    s.onFrame(frame(20000, {ct(0, false, 200, 200)}));  // quick lift
    CHECK_EQ(inj.clicks(), 1);
    CHECK_EQ(inj.moves, 0);
    // Order: down then up.
    CHECK_EQ(inj.events.size(), static_cast<std::size_t>(2));
    if (inj.events.size() == 2) {
        CHECK(inj.events[0] == "left_down");
        CHECK(inj.events[1] == "left_up");
    }
}

TEST_CASE("press that travels far is not a tap") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 100, 100)}));
    s.onFrame(frame(5000, {ct(0, true, 140, 100)}));    // +40 -> travel > threshold
    s.onFrame(frame(10000, {ct(0, false, 140, 100)}));  // lift
    CHECK_EQ(inj.clicks(), 0);
    CHECK(inj.moves >= 1);
}

TEST_CASE("long press then move drags: left_down, moves, left_up in order") {
    MockInjector inj;
    MouseSink s(inj);  // longPressDragUs=400ms, dragStartMovePx=16 (defaults)
    s.onFrame(frame(0, {ct(0, true, 300, 300)}));            // anchor
    s.onFrame(frame(200000, {ct(0, true, 302, 301)}));       // held still (jitter < 16px)
    s.onFrame(frame(450000, {ct(0, true, 303, 301)}));       // > 400ms still held -> arm drag
    s.onFrame(frame(470000, {ct(0, true, 340, 320)}));       // now drag-move
    s.onFrame(frame(490000, {ct(0, true, 380, 350)}));       // keep dragging
    s.onFrame(frame(510000, {ct(0, false, 380, 350)}));      // lift -> drag end
    CHECK_EQ(inj.count("left_down"), 1);                     // pressed once
    CHECK_EQ(inj.count("left_up"), 1);                       // released once
    CHECK(inj.moves >= 2);                                   // dragged while held
    // Order: down precedes the drag moves, up is last.
    std::size_t downIdx = inj.events.size(), upIdx = 0, lastMoveIdx = 0;
    for (std::size_t i = 0; i < inj.events.size(); ++i) {
        if (inj.events[i] == "left_down") downIdx = i;
        else if (inj.events[i] == "left_up") upIdx = i;
        else if (inj.events[i].rfind("move ", 0) == 0) lastMoveIdx = i;
    }
    CHECK(downIdx < lastMoveIdx);
    CHECK(lastMoveIdx < upIdx);
}

TEST_CASE("short tap before the long-press timer stays a left click, no drag") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 200, 200)}));
    s.onFrame(frame(20000, {ct(0, false, 200, 200)}));  // quick lift, well under 400ms
    CHECK_EQ(inj.clicks(), 1);              // ordinary tap = one left click
    CHECK_EQ(inj.count("left_up"), 1);      // the click's own up, not a drag release
    CHECK_EQ(inj.moves, 0);
}

TEST_CASE("armed drag released on lift via empty frame (disconnect/emitLift path)") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 300, 300)}));        // anchor
    s.onFrame(frame(450000, {ct(0, true, 301, 300)}));   // hold -> arm (left_down)
    CHECK_EQ(inj.count("left_down"), 1);
    s.onFrame(TouchFrame{});                              // empty all-lifted frame
    CHECK_EQ(inj.count("left_up"), 1);                   // drag always released
    CHECK_EQ(inj.count("left_down"), 1);                 // balanced, no extra down
}

TEST_CASE("second finger during a drag releases the button, then freezes") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 300, 300)}));        // anchor
    s.onFrame(frame(450000, {ct(0, true, 301, 300)}));   // arm drag (left_down)
    const int movesBefore = inj.moves;
    // Second finger lands -> multi-touch interrupts: release, freeze (no cursor jump).
    s.onFrame(frame(470000, {ct(0, true, 301, 300), ct(1, true, 500, 500)}));
    CHECK_EQ(inj.count("left_up"), 1);
    CHECK_EQ(inj.moves, movesBefore);
}

TEST_CASE("plain single-finger move never arms a drag (no left_down)") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 100, 100)}));        // anchor
    s.onFrame(frame(50000, {ct(0, true, 140, 110)}));    // +40px travel before timer
    s.onFrame(frame(450000, {ct(0, true, 200, 160)}));   // still moving, past 400ms
    s.onFrame(frame(500000, {ct(0, false, 200, 160)}));  // lift
    CHECK_EQ(inj.count("left_down"), 0);  // travel > dragStartMovePx -> Moving, never drag
    CHECK_EQ(inj.count("left_up"), 0);
    CHECK_EQ(inj.clicks(), 0);            // travelled too far to be a tap either
    CHECK(inj.moves >= 1);
}

TEST_CASE("multi-touch freezes cursor and resets anchor (no jump)") {
    MockInjector inj;
    MouseSink s(inj);
    s.onFrame(frame(0, {ct(0, true, 100, 100)}));       // anchor
    s.onFrame(frame(1000, {ct(0, true, 110, 100)}));    // +10
    const int movesBefore = inj.moves;

    // Second finger down -> 2 active contacts -> freeze.
    s.onFrame(frame(2000, {ct(0, true, 200, 100), ct(1, true, 300, 300)}));
    CHECK_EQ(inj.moves, movesBefore);  // no move while multi-touch

    // Back to one finger far away -> new anchor, must NOT emit the 90px jump.
    s.onFrame(frame(3000, {ct(0, true, 210, 100)}));
    CHECK_EQ(inj.moves, movesBefore);

    // Continue moving -> normal delta resumes.
    s.onFrame(frame(4000, {ct(0, true, 216, 100)}));    // +6
    CHECK_EQ(inj.sumDx, 16L);  // only the 10 + 6, never the jumps
    CHECK_EQ(inj.clicks(), 0);
}
