// ClientService (v0.3.0): the headless, UI-agnostic engine that both the CLI
// (phone2pad_client) and the tray (phone2pad_tray) drive. It owns the full Phase A/B
// pipeline (AdbManager -> FrameReceiver -> OrientationNormalizingSink -> GestureRouter
// -> MouseSink/GestureSink -> Win32InputInjector) and runs the device/connection
// lifecycle on its own thread, exposing a single consolidated ServiceState plus a
// transition callback for the front-end to render.
//
// Idle/low-power model (a tiered loop, never a busy-wait):
//   - Monitor (idle): no usable device -> poll `adb devices` at a conservative cadence
//     (devicePollMs, with a gentle backoff toward devicePollMaxMs). No sockets, no
//     input pipeline activity. State = NoDevice/Unauthorized/...
//   - Armed: a device is Ready and the forward is set, but the phone app has not
//     entered Trackpad Mode yet -> probe the forwarded socket every appPollMs,
//     sleeping between probes. State = WaitingForApp.
//   - Active: the app is streaming -> frames pump at full speed (no artificial sleep).
//     This is the ONLY phase where the input pipeline is live. State = Connected.
// On disconnect the loop emits a lift frame and drops back to Monitor/Armed.
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "phone2pad/client/adb_manager.hpp"
#include "phone2pad/client/frame_receiver.hpp"
#include "phone2pad/client/gesture_router.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "phone2pad/client/mouse_sink.hpp"
#include "phone2pad/client/orientation_normalizer.hpp"
#include "phone2pad/client/win32_input_injector.hpp"

namespace phone2pad::client {

// Consolidated, front-end-renderable status. Derived from AdbManager::queryDevices()
// plus the forward/first-data signals — no new detection logic, just a single surface.
enum class ServiceState {
    Stopped,          // worker not running
    AdbMissing,       // no adb.exe found on disk
    NoDevice,         // no phone connected (idle monitor tier)
    Unauthorized,     // phone present but USB debugging not authorized
    DeviceOffline,    // phone present but offline (still connecting)
    MultipleDevices,  // more than one usable phone (ambiguous)
    ForwardFailed,    // adb forward setup failed
    WaitingForApp,    // armed: device ready + forwarded, app not in Trackpad Mode yet
    Connected,        // active: phone app streaming, pipeline live
};

// One-line human description for tooltips / logging (never null, stable text).
const char* describe(ServiceState s);

// One of the four user-facing buckets the tray shows:
//   "Idle" (Stopped / NoDevice)            -> nothing to do, low power
//   "Waiting for phone" (device seen but not usable: Unauthorized/Offline/Multiple/
//                        ForwardFailed)
//   "Waiting for app" (WaitingForApp)
//   "Connected" (Connected)
// AdbMissing returns "adb not found" (a setup problem, surfaced distinctly).
const char* shortLabel(ServiceState s);

struct ClientServiceConfig {
    std::string package = "com.phone2pad";
    std::string activity = ".BlackPadActivity";
    int port = 38917;
    MouseSinkConfig mouse;
    GestureConfig gesture;

    // Idle/low-power cadences (milliseconds). Conservative by design — no tight loops.
    int devicePollMs = 2000;     // monitor tier: `adb devices` poll interval
    int devicePollMaxMs = 5000;  // backoff cap while the idle state is unchanged
    int appPollMs = 1000;        // armed tier: forwarded-socket probe interval
};

class ClientService {
public:
    using StateCallback = std::function<void(ServiceState)>;

    explicit ClientService(ClientServiceConfig config = {});
    ~ClientService();

    ClientService(const ClientService&) = delete;
    ClientService& operator=(const ClientService&) = delete;

    // Register before start(). Invoked on every state transition from the worker
    // thread; front-ends that touch UI from another thread should marshal (the tray
    // PostMessage()s to its UI thread, the CLI prints directly).
    void setStateCallback(StateCallback cb);

    void start();  // idempotent: spins the worker thread (no-op if already running)
    void stop();   // idempotent: signals stop and joins the worker

    // Stop the worker, re-run adb discovery, then start again. Lets a front-end retry
    // after the user fixes setup (e.g. installs adb) without relaunching the process.
    // Synchronous: stop() joins the worker before adb is re-checked, so there is no
    // race and no orphaned work; adbReady() is accurate on return.
    void restart();

    bool running() const { return running_.load(); }
    ServiceState state() const;

    // adb discovery results (for front-end setup messaging). Valid after construction.
    bool adbReady() const { return adb_.ready(); }
    const std::string& adbPath() const { return adb_.adbPath(); }
    const std::string& adbSource() const { return adb_.adbSource(); }

    // Snapshot of PING RTT samples (ms) for --latency-report. Safe after stop().
    std::vector<double> rttSamplesMs() const;

private:
    void run();  // worker loop (tiered monitor -> arm -> active)
    void setState(ServiceState s);

    ClientServiceConfig cfg_;

    // Pipeline, constructed once. Holding the sinks while idle is cheap; the tiered
    // loop is what keeps idle power near zero (no polling/socket churn).
    Win32InputInjector injector_;
    MouseSink mouse_;
    GestureSink gesture_;
    GestureRouter router_;
    OrientationNormalizingSink normalizer_;
    FrameReceiver receiver_;
    AdbManager adb_;

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};

    mutable std::mutex mtx_;
    ServiceState state_ = ServiceState::Stopped;
    StateCallback cb_;
};

}  // namespace phone2pad::client
