// phone2pad_client (CLI): the console front-end. It parses flags, then drives a
// ClientService — the shared engine that polls adb, sets up the port forward, and
// streams phone touch frames into the sinks (relative mouse + gestures). Status is
// printed from the service's state-change callback. `--latency-report` prints PING
// RTT percentiles on exit.
//
// The client does NOT auto-launch the phone app. The official flow is manual: run
// this client, then on the phone open phone2pad and tap "Trackpad Mode Start"
// (BlackPadActivity). Developers can still launch the pad manually with
// `adb shell am start -n com.phone2pad/.BlackPadActivity`.
//
// v0.3.0: for a no-console, stays-in-the-tray experience, use phone2pad_tray.exe
// instead — it drives the same ClientService and adds autostart-on-login.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "phone2pad/client/client_service.hpp"

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

void printLatency(const std::vector<double>& s) {
    if (s.empty()) {
        std::printf("latency: no PONG samples\n");
        return;
    }
    std::printf("latency: n=%zu  p50=%.2fms  p95=%.2fms  p99=%.2fms\n", s.size(),
                percentile(s, 0.50), percentile(s, 0.95), percentile(s, 0.99));
}

// Shown when no adb.exe is found anywhere. Concise, non-technical, with the one
// official download URL and the app-local drop-in location.
void printAdbMissing() {
    std::printf(
        "Android Platform Tools (adb) were not found.\n"
        "phone2pad uses adb only for the local USB link between your phone and PC -\n"
        "nothing is sent online.\n\n"
        "Fix it one of two ways:\n"
        "  1) Install Android Platform Tools and make sure adb is on your PATH, or\n"
        "  2) Download them and unzip into a \"platform-tools\" folder next to\n"
        "     phone2pad_client.exe.\n\n"
        "Download: https://developer.android.com/tools/releases/platform-tools\n"
        "See ADB-SETUP.md (next to the exe) for step-by-step instructions.\n");
}

// One concise, non-technical message per service state. Mirrors the v0.2.2 CLI
// wording; driven by ClientService's state-change callback.
void printState(ServiceState state, ServiceState prev) {
    switch (state) {
        case ServiceState::NoDevice:
            std::printf(
                "No phone detected. Connect your phone via USB and turn on USB debugging.\n");
            break;
        case ServiceState::Unauthorized:
            std::printf(
                "Phone connected but not authorized. Unlock it and tap Allow on the\n"
                "\"Allow USB debugging?\" prompt.\n");
            break;
        case ServiceState::DeviceOffline:
            std::printf(
                "Phone is still connecting (offline). Reconnect the USB cable if this\n"
                "does not clear up.\n");
            break;
        case ServiceState::MultipleDevices:
            std::printf(
                "More than one phone is connected. Disconnect the others and run\n"
                "phone2pad again.\n");
            break;
        case ServiceState::ForwardFailed:
            std::printf(
                "Couldn't set up the USB connection (adb forward). Reconnect the cable\n"
                "and try again.\n");
            break;
        case ServiceState::WaitingForApp:
            if (prev == ServiceState::Connected) {
                std::printf("disconnected; waiting for reconnect...\n");
            }
            std::printf(
                "Open phone2pad on your Android phone and tap Trackpad Mode Start.\n");
            break;
        case ServiceState::Connected:
            std::printf("Connected. Your phone is now a trackpad.\n");
            break;
        case ServiceState::AdbMissing:
        case ServiceState::Stopped:
            break;  // handled around start()/shutdown
    }
}

}  // namespace

int main(int argc, char** argv) {
    ClientServiceConfig cfg;
    bool latencyReport = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--package") {
            cfg.package = next();
        } else if (a == "--port") {
            cfg.port = std::atoi(next().c_str());
        } else if (a == "--sensitivity") {
            cfg.mouse.sensitivity = std::atof(next().c_str());
        } else if (a == "--scroll-traditional") {
            cfg.gesture.naturalScroll = false;
        } else if (a == "--scroll-sensitivity") {
            cfg.gesture.scrollPixelsPerNotch = std::atoi(next().c_str());
        } else if (a == "--no-pinch") {
            cfg.gesture.enablePinch = false;
        } else if (a == "--latency-report") {
            latencyReport = true;
        } else if (a == "--help") {
            std::printf(
                "usage: phone2pad_client [--package P] [--port N] [--sensitivity F] "
                "[--scroll-traditional] [--scroll-sensitivity PX] [--no-pinch] "
                "[--latency-report]\n");
            return 0;
        }
    }

#ifdef _WIN32
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#endif
    // Unbuffered stdout so the waiting/status lines show up promptly, including when
    // the output is piped or logged (block-buffered by default in that case).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    ClientService service(cfg);

    std::printf("phone2pad: turning your USB-connected phone into a Windows trackpad.\n");
    if (!service.adbReady()) {
        printAdbMissing();
        return 1;
    }
    std::printf("Found adb (%s): %s\n", service.adbSource().c_str(),
                service.adbPath().c_str());
    std::printf("Waiting for your phone (port %d)...\n", cfg.port);

    std::atomic<ServiceState> prev{ServiceState::Stopped};
    service.setStateCallback([&prev](ServiceState s) {
        printState(s, prev.exchange(s));
    });
    service.start();

    while (!g_stop.load()) std::this_thread::sleep_for(100ms);

    service.stop();
    if (latencyReport) printLatency(service.rttSamplesMs());
    return 0;
}
