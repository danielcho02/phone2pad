// replay (docs/05 §4): replay a .pptrace into a Sink, preserving timing. Lets the
// PC side be exercised without a phone.
//   replay --trace x.pptrace --sink mouse|gesture [--speed 1.0] [--dry-run]
//          [--scroll-traditional] [--scroll-sensitivity PX] [--no-pinch]
// --dry-run logs injected input instead of moving the real cursor.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "phone2pad/client/gesture_router.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "phone2pad/client/input_injector.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/replay.hpp"
#include "phone2pad/client/trace.hpp"
#include "phone2pad/client/win32_input_injector.hpp"

using namespace phone2pad::client;

namespace {

// Prints injected events instead of calling SendInput (phone-free, side-effect-free).
class LoggingInjector : public InputInjector {
public:
    void moveRelative(int dx, int dy) override { std::printf("move %d %d\n", dx, dy); }
    void leftDown() override { std::printf("left_down\n"); }
    void leftUp() override { std::printf("left_up\n"); }
    void rightDown() override { std::printf("right_down\n"); }
    void rightUp() override { std::printf("right_up\n"); }
    void wheel(int delta) override { std::printf("wheel %+d\n", delta); }
    void hwheel(int delta) override { std::printf("hwheel %+d\n", delta); }
    void keyDown(int vk) override { std::printf("key_down 0x%02X\n", vk); }
    void keyUp(int vk) override { std::printf("key_up 0x%02X\n", vk); }
};

}  // namespace

int main(int argc, char** argv) {
    std::string tracePath;
    std::string sinkName = "mouse";
    double speed = 1.0;
    bool dryRun = false;
    GestureConfig gcfg;

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
        } else if (a == "--scroll-traditional") {
            gcfg.naturalScroll = false;
        } else if (a == "--scroll-sensitivity") {
            gcfg.scrollPixelsPerNotch = std::atoi(next().c_str());
        } else if (a == "--no-pinch") {
            gcfg.enablePinch = false;
        } else if (a == "--help") {
            std::printf(
                "usage: replay --trace x.pptrace --sink mouse|gesture [--speed F] "
                "[--dry-run] [--scroll-traditional] [--scroll-sensitivity PX] "
                "[--no-pinch]\n");
            return 0;
        }
    }

    if (tracePath.empty()) {
        std::fprintf(stderr, "error: --trace is required\n");
        return 2;
    }
    if (sinkName != "mouse" && sinkName != "gesture") {
        std::fprintf(stderr, "error: --sink must be 'mouse' or 'gesture'\n");
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

    MouseSink mouse(injector);
    if (sinkName == "mouse") {
        replayFrames(frames, mouse, speed, realSleepMicros);
    } else {
        GestureSink gesture(injector, gcfg);
        GestureRouter router(mouse, gesture);
        replayFrames(frames, router, speed, realSleepMicros);
    }
    std::printf("replayed %zu frames from %s\n", frames.size(), tracePath.c_str());
    return 0;
}
