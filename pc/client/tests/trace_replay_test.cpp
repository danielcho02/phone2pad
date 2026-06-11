// L3 (docs/05 §4): replay the synthetic .pptrace fixtures through MouseSink with a
// MockInjector — exercises the full PC pipeline (trace -> replay -> sink) phone-free.
#include <string>

#include "mock_injector.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/replay.hpp"
#include "phone2pad/client/trace.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;

namespace {

std::string fixture(const char* name) {
    return std::string(PHONE2PAD_FIXTURE_DIR) + "/" + name;
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
