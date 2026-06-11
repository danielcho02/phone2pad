// recorder (docs/05 §5): connect to the phone (via adb forward) and save received
// touch frames to a .pptrace file for phone-free replay/testing.
//   recorder --out traces/<name>.pptrace [--port 38917] [--package P]
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

#include "phone2pad/client/adb_manager.hpp"
#include "phone2pad/client/frame_receiver.hpp"
#include "phone2pad/client/net_client.hpp"
#include "phone2pad/client/sink.hpp"
#include "phone2pad/client/trace.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace phone2pad::client;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> g_stop{false};

#ifdef _WIN32
BOOL WINAPI ctrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT) {
        g_stop.store(true);
        return TRUE;
    }
    return FALSE;
}
#endif

// Writes each received frame as one .pptrace line.
class RecorderSink : public Sink {
public:
    explicit RecorderSink(const std::string& path) : out_(path) {}
    bool ok() const { return static_cast<bool>(out_); }
    std::size_t count() const { return count_; }

    void onFrame(const phone2pad::proto::TouchFrame& frame) override {
        out_ << toTraceLine(frame.timestampUs, frame) << '\n';
        out_.flush();
        ++count_;
    }

private:
    std::ofstream out_;
    std::size_t count_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
    std::string outPath;
    std::string package = "com.phone2pad";
    int port = 38917;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--out") {
            outPath = next();
        } else if (a == "--port") {
            port = std::atoi(next().c_str());
        } else if (a == "--package") {
            package = next();
        } else if (a == "--help") {
            std::printf("usage: recorder --out FILE [--port N] [--package P]\n");
            return 0;
        }
    }

    if (outPath.empty()) {
        std::fprintf(stderr, "error: --out is required\n");
        return 2;
    }

    RecorderSink sink(outPath);
    if (!sink.ok()) {
        std::fprintf(stderr, "error: cannot open '%s' for writing\n", outPath.c_str());
        return 1;
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#endif

    FrameReceiver receiver(sink);
    AdbManager adb(package, ".BlackPadActivity", port);
    std::printf("recorder: writing %s (Ctrl-C to stop)\n", outPath.c_str());

    while (!g_stop.load()) {
        const auto device = adb.firstDevice();
        if (!device) {
            std::this_thread::sleep_for(1s);
            continue;
        }
        adb.setupForward(*device);
        adb.launchApp(*device);
        std::this_thread::sleep_for(500ms);
        runClientConnection(receiver, port, g_stop);
    }

    std::printf("recorded %zu frames\n", sink.count());
    return 0;
}
