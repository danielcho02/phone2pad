// phone2pad_client: poll adb for a device, set up the port forward, then wait for
// the user to start trackpad mode on the phone and stream touch frames into the
// sinks (relative mouse + gestures). On disconnect it re-polls.
// `--latency-report` prints PING RTT percentiles.
//
// The client does NOT auto-launch the phone app. The official v0.2.0 flow is
// manual: run this client, then on the phone open phone2pad and tap
// "Trackpad Mode Start" (BlackPadActivity). Developers can still launch the pad
// manually with `adb shell am start -n com.phone2pad/.BlackPadActivity`.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "phone2pad/client/adb_manager.hpp"
#include "phone2pad/client/frame_receiver.hpp"
#include "phone2pad/client/gesture_router.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/net_client.hpp"
#include "phone2pad/client/orientation_normalizer.hpp"
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

// Shown when no adb.exe is found anywhere. Concise, non-technical, with the one
// official download URL and the app-local drop-in location.
void printAdbMissing() {
    std::printf(
        "Android Platform Tools (adb) were not found.\n"
        "phone2pad uses adb only for the local USB link between your phone and PC -\n"
        "nothing is sent online.\n\n"
        "Fix it one of two ways:\n"
        "  1) Install Android Platform Tools and make sure adb is on your PATH, or\n"
        "  2) Download them and unzip into a \"tools\\platform-tools\" folder next to\n"
        "     phone2pad_client.exe.\n\n"
        "Download: https://developer.android.com/tools/releases/platform-tools\n");
}

// One concise, non-technical line per device-state problem.
void printDeviceStatus(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::None:
            std::printf(
                "No phone detected. Connect your phone via USB and turn on USB debugging.\n");
            break;
        case DeviceStatus::Unauthorized:
            std::printf(
                "Phone connected but not authorized. Unlock it and tap Allow on the\n"
                "\"Allow USB debugging?\" prompt.\n");
            break;
        case DeviceStatus::Offline:
            std::printf(
                "Phone is still connecting (offline). Reconnect the USB cable if this\n"
                "does not clear up.\n");
            break;
        case DeviceStatus::Multiple:
            std::printf(
                "More than one phone is connected. Disconnect the others and run\n"
                "phone2pad again.\n");
            break;
        case DeviceStatus::Ready:
            break;  // handled inline (forward setup)
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string package = "com.phone2pad";
    std::string activity = ".BlackPadActivity";
    int port = 38917;
    double sensitivity = 1.0;
    bool latencyReport = false;
    GestureConfig gcfg;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--package") {
            package = next();
        } else if (a == "--port") {
            port = std::atoi(next().c_str());
        } else if (a == "--sensitivity") {
            sensitivity = std::atof(next().c_str());
        } else if (a == "--scroll-traditional") {
            gcfg.naturalScroll = false;
        } else if (a == "--scroll-sensitivity") {
            gcfg.scrollPixelsPerNotch = std::atoi(next().c_str());
        } else if (a == "--no-pinch") {
            gcfg.enablePinch = false;
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

    Win32InputInjector injector;
    MouseSinkConfig cfg;
    cfg.sensitivity = sensitivity;
    MouseSink mouse(injector, cfg);
    GestureSink gesture(injector, gcfg);
    GestureRouter router(mouse, gesture);  // 1 finger -> mouse, 2+ -> gestures
    // Normalize the two landscape orientations onto the canonical frame (USB on the right)
    // before any sink sees the frame, so the same physical gesture is consistent after a
    // 180° flip. Rotation/dims come from HELLO (re-sent on rotation).
    OrientationNormalizingSink normalizer(router);
    FrameReceiver receiver(normalizer);
    receiver.setHelloHandler([&normalizer](const phone2pad::proto::Hello& h) {
        normalizer.setOrientation(h.rotation, h.screenWidthPx, h.screenHeightPx);
    });
    AdbManager adb(package, activity, port);

    std::printf("phone2pad: turning your USB-connected phone into a Windows trackpad.\n");
    if (!adb.ready()) {
        printAdbMissing();
        return 1;
    }
    std::printf("Found adb (%s): %s\n", adb.adbSource().c_str(), adb.adbPath().c_str());
    std::printf("Waiting for your phone (port %d)...\n", port);

    std::string forwardedSerial;             // device the forward is currently set up for
    bool hintShown = false;                  // "tap Trackpad Mode Start" shown for this wait
    std::optional<DeviceStatus> lastStatus;  // last reported status (latched, prints once)

    while (!g_stop.load()) {
        const DeviceQuery q = adb.queryDevices();
        if (q.status != DeviceStatus::Ready) {
            if (lastStatus != q.status) {
                printDeviceStatus(q.status);
                lastStatus = q.status;
            }
            forwardedSerial.clear();
            hintShown = false;
            std::this_thread::sleep_for(1s);
            continue;
        }
        lastStatus = DeviceStatus::Ready;

        // Set up the adb forward once per device (idempotent; no app auto-launch).
        if (forwardedSerial != q.serial) {
            std::printf("Phone detected (%s). Setting up the USB connection...\n",
                        q.serial.c_str());
            if (!adb.setupForward(q.serial)) {
                std::printf(
                    "Couldn't set up the USB connection (adb forward). Reconnect the cable\n"
                    "and try again.\n");
                forwardedSerial.clear();
                std::this_thread::sleep_for(1s);
                continue;
            }
            forwardedSerial = q.serial;
            hintShown = false;
        }
        if (!hintShown) {
            std::printf("Open phone2pad on your Android phone and tap Trackpad Mode Start.\n");
            hintShown = true;
        }

        // Try to connect and pump frames. With adb forward, connect() succeeds even
        // before the user taps Trackpad Mode Start (the forward accepts then closes
        // with no data); `gotData` tells a real session from that idle case, so we
        // poll quietly instead of spamming the hint / empty latency reports.
        bool gotData = false;
        runClientConnection(receiver, port, g_stop, 1000, &gotData);
        if (gotData) {
            std::printf("disconnected; waiting for reconnect...\n");
            if (latencyReport) printLatency(receiver);
            hintShown = false;  // re-show the hint while waiting to reconnect
        } else {
            std::this_thread::sleep_for(500ms);  // app not started yet; keep polling
        }
    }

    if (latencyReport) printLatency(receiver);
    return 0;
}
