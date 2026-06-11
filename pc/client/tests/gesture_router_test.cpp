// GestureRouter unit tests (docs/05 L2): the router must route 1-finger sessions to
// MouseSink unchanged, route 2+ finger sessions to GestureSink, and — the Phase A
// regression guard — never let the 2->1 finger decay tail of a gesture leak back into
// cursor movement (no jump).
#include <vector>

#include "mock_injector.hpp"
#include "phone2pad/client/gesture_router.hpp"
#include "phone2pad/client/gesture_sink.hpp"
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

TEST_CASE("single-finger session is identical to MouseSink alone") {
    const std::vector<TouchFrame> frames = {
        frame(0, {ct(0, true, 100, 100)}),
        frame(1000, {ct(0, true, 112, 108)}),
        frame(2000, {ct(0, true, 120, 118)}),
        frame(3000, {ct(0, false, 120, 118)}),
    };

    MockInjector direct;
    MouseSink mouseOnly(direct);
    for (const auto& f : frames) mouseOnly.onFrame(f);

    MockInjector routed, gest;
    MouseSink mouse(routed);
    GestureSink gesture(gest);
    GestureRouter router(mouse, gesture);
    for (const auto& f : frames) router.onFrame(f);

    CHECK_EQ(routed.events, direct.events);  // byte-for-byte same mouse behaviour
    CHECK(gest.events.empty());              // gesture sink never saw a 1-finger frame
}

TEST_CASE("two-finger gesture routes to GestureSink, cursor stays put") {
    MockInjector mouseInj, gestInj;
    MouseSink mouse(mouseInj);
    GestureSink gesture(gestInj);
    GestureRouter router(mouse, gesture);

    const std::vector<TouchFrame> scroll = {
        frame(0, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),
        frame(100000, {ct(0, true, 200, 300), ct(1, true, 400, 300)}),  // settle window
        frame(150000, {ct(0, true, 200, 240), ct(1, true, 400, 240)}),
        frame(200000, {ct(0, true, 200, 160), ct(1, true, 400, 160)}),
        frame(250000, {ct(0, false, 200, 160), ct(1, false, 400, 160)}),
    };
    for (const auto& f : scroll) router.onFrame(f);

    CHECK(gestInj.wheelEvents() > 0);   // gesture handled the scroll
    CHECK_EQ(mouseInj.moves, 0);        // cursor did not move during the gesture
}

TEST_CASE("2->1 finger decay tail does not move the cursor (no jump)") {
    MockInjector mouseInj, gestInj;
    MouseSink mouse(mouseInj);
    GestureSink gesture(gestInj);
    GestureRouter router(mouse, gesture);

    const std::vector<TouchFrame> frames = {
        frame(0, {ct(0, true, 100, 100)}),                       // 1 finger anchor
        frame(1000, {ct(0, true, 110, 100)}),                    // +10 mouse move
        frame(2000, {ct(0, true, 200, 100), ct(1, true, 300, 300)}),  // 2 fingers -> gesture
        frame(3000, {ct(0, true, 210, 100)}),                    // decays to 1 (far away)
        frame(4000, {ct(0, true, 240, 100)}),                    // still 1 finger, moving
        frame(5000, {ct(0, false, 240, 100)}),                   // all lift -> session end
        frame(6000, {ct(0, true, 300, 100)}),                    // new 1-finger session anchor
        frame(7000, {ct(0, true, 306, 100)}),                    // +6 mouse move
    };
    for (const auto& f : frames) router.onFrame(f);

    CHECK_EQ(mouseInj.sumDx, 16L);  // only 10 + 6; the 90px / 30px jumps never leak
    CHECK_EQ(mouseInj.moves, 2);
}
