// replay (docs/05 §4): replay a .pptrace into a Sink, preserving timing. Lets the
// PC side be exercised without a phone.
//   replay --trace x.pptrace --sink mouse [--speed 1.0] [--dry-run]
// --dry-run logs injected input instead of moving the real cursor.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "phantompad/client/input_injector.hpp"
#include "phantompad/client/mouse_sink.hpp"
#include "phantompad/client/replay.hpp"
#include "phantompad/client/trace.hpp"
#include "phantompad/client/win32_input_injector.hpp"

using namespace phantompad::client;

namespace {

// Prints injected events instead of calling SendInput (phone-free, side-effect-free).
class LoggingInjector : public InputInjector {
public:
    void moveRelative(int dx, int dy) override { std::printf("move %d %d\n", dx, dy); }
    void leftDown() override { std::printf("left_down\n"); }
    void leftUp() override { std::printf("left_up\n"); }
};

}  // namespace

int main(int argc, char** argv) {
    std::string tracePath;
    std::string sinkName = "mouse";
    double speed = 1.0;
    bool dryRun = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--trace") {
            tracePath = next();
        } else if (a == "--sink") {
            sinkName = next();
        } else if (a == "--speed") {
            speed = std::atof(next().c_str());
        } else if (a == "--dry-run") {
            dryRun = true;
        } else if (a == "--help") {
            std::printf("usage: replay --trace x.pptrace --sink mouse [--speed F] [--dry-run]\n");
            return 0;
        }
    }

    if (tracePath.empty()) {
        std::fprintf(stderr, "error: --trace is required\n");
        return 2;
    }
    if (sinkName != "mouse") {
        std::fprintf(stderr, "error: Phase A only supports --sink mouse\n");
        return 2;
    }

    std::vector<TimedFrame> frames;
    try {
        frames = readTrace(tracePath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }

    Win32InputInjector realInjector;
    LoggingInjector logInjector;
    InputInjector& injector =
        dryRun ? static_cast<InputInjector&>(logInjector) : realInjector;

    MouseSink sink(injector);
    replayFrames(frames, sink, speed, realSleepMicros);
    std::printf("replayed %zu frames from %s\n", frames.size(), tracePath.c_str());
    return 0;
}
