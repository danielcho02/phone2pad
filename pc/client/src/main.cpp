// phone2pad_client (Phase A): poll adb for a device, set up the port forward,
// launch the phone app, then stream touch frames into MouseSink (relative mouse +
// tap). On disconnect it re-polls. `--latency-report` prints PING RTT percentiles.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "phone2pad/client/adb_manager.hpp"
#include "phone2pad/client/frame_receiver.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/net_client.hpp"
#include "phone2pad/client/win32_input_injector.hpp"

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

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t idx = static_cast<std::size_t>(p * (v.size() - 1) + 0.5);
    return v[std::min(idx, v.size() - 1)];
}

void printLatency(const FrameReceiver& receiver) {
    const auto& s = receiver.rttSamplesMs();
    if (s.empty()) {
        std::printf("latency: no PONG samples\n");
        return;
    }
    std::printf("latency: n=%zu  p50=%.2fms  p95=%.2fms  p99=%.2fms\n", s.size(),
                percentile(s, 0.50), percentile(s, 0.95), percentile(s, 0.99));
}

}  // namespace

int main(int argc, char** argv) {
    std::string package = "com.phone2pad";
    std::string activity = ".BlackPadActivity";
    int port = 38917;
    double sensitivity = 1.0;
    bool latencyReport = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--package") {
            package = next();
        } else if (a == "--port") {
            port = std::atoi(next().c_str());
        } else if (a == "--sensitivity") {
            sensitivity = std::atof(next().c_str());
        } else if (a == "--latency-report") {
            latencyReport = true;
        } else if (a == "--help") {
            std::printf(
                "usage: phone2pad_client [--package P] [--port N] "
                "[--sensitivity F] [--latency-report]\n");
            return 0;
        }
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#endif

    Win32InputInjector injector;
    MouseSinkConfig cfg;
    cfg.sensitivity = sensitivity;
    MouseSink sink(injector, cfg);
    FrameReceiver receiver(sink);
    AdbManager adb(package, activity, port);

    std::printf("phone2pad client: adb=%s port=%d package=%s\n", adb.adbPath().c_str(), port,
                package.c_str());

    while (!g_stop.load()) {
        const auto device = adb.firstDevice();
        if (!device) {
            std::this_thread::sleep_for(1s);
            continue;
        }
        std::printf("device %s: forwarding + launching app\n", device->c_str());
        adb.setupForward(*device);
        adb.launchApp(*device);
        std::this_thread::sleep_for(500ms);  // let the phone bind its server

        const bool connected = runClientConnection(receiver, port, g_stop);
        if (!connected) std::this_thread::sleep_for(1s);
        if (latencyReport) printLatency(receiver);
    }

    if (latencyReport) printLatency(receiver);
    return 0;
}
