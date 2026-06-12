#include "phone2pad/client/client_service.hpp"

#include <algorithm>
#include <chrono>

#include "phone2pad/client/net_client.hpp"

namespace phone2pad::client {

namespace {

// Sleep up to `ms`, waking early (in <=100ms chunks) when `stop` is set, so stop()
// stays responsive without busy-waiting. Idle CPU is dominated by these sleeps.
void sleepResponsive(int ms, const std::atomic<bool>& stop) {
    constexpr int kStep = 100;
    int slept = 0;
    while (slept < ms && !stop.load()) {
        const int chunk = std::min(kStep, ms - slept);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        slept += chunk;
    }
}

ServiceState mapNonReady(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::None:
            return ServiceState::NoDevice;
        case DeviceStatus::Unauthorized:
            return ServiceState::Unauthorized;
        case DeviceStatus::Offline:
            return ServiceState::DeviceOffline;
        case DeviceStatus::Multiple:
            return ServiceState::MultipleDevices;
        case DeviceStatus::Ready:
            break;  // not a non-ready status; caller handles Ready separately
    }
    return ServiceState::NoDevice;
}

}  // namespace

const char* describe(ServiceState s) {
    switch (s) {
        case ServiceState::Stopped:
            return "Stopped.";
        case ServiceState::AdbMissing:
            return "Android Platform Tools (adb) not found.";
        case ServiceState::NoDevice:
            return "Waiting for a phone (connect USB + enable USB debugging).";
        case ServiceState::Unauthorized:
            return "Phone connected but not authorized (tap Allow on the phone).";
        case ServiceState::DeviceOffline:
            return "Phone is still connecting (offline).";
        case ServiceState::MultipleDevices:
            return "More than one phone connected; leave only one.";
        case ServiceState::ForwardFailed:
            return "Could not set up the USB connection (adb forward).";
        case ServiceState::WaitingForApp:
            return "Ready — open phone2pad on the phone and tap Trackpad Mode Start.";
        case ServiceState::Connected:
            return "Connected — your phone is acting as a trackpad.";
    }
    return "";
}

const char* shortLabel(ServiceState s) {
    switch (s) {
        case ServiceState::Stopped:
        case ServiceState::NoDevice:
            return "Idle";
        case ServiceState::AdbMissing:
            return "adb not found";
        case ServiceState::Unauthorized:
        case ServiceState::DeviceOffline:
        case ServiceState::MultipleDevices:
        case ServiceState::ForwardFailed:
            return "Waiting for phone";
        case ServiceState::WaitingForApp:
            return "Waiting for app";
        case ServiceState::Connected:
            return "Connected";
    }
    return "Idle";
}

ClientService::ClientService(ClientServiceConfig config)
    : cfg_(std::move(config)),
      injector_(),
      mouse_(injector_, cfg_.mouse),
      gesture_(injector_, cfg_.gesture),
      router_(mouse_, gesture_),       // 1 finger -> mouse, 2+ -> gestures
      normalizer_(router_),            // canonicalize landscape orientation first
      receiver_(normalizer_),
      adb_(cfg_.package, cfg_.activity, cfg_.port) {
    // Push the phone's rotation/dims (HELLO, re-sent on rotation) into the normalizer.
    receiver_.setHelloHandler([this](const phone2pad::proto::Hello& h) {
        normalizer_.setOrientation(h.rotation, h.screenWidthPx, h.screenHeightPx);
    });
}

ClientService::~ClientService() { stop(); }

void ClientService::setStateCallback(StateCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    cb_ = std::move(cb);
}

void ClientService::start() {
    if (running_.exchange(true)) return;  // already running
    stop_.store(false);
    worker_ = std::thread([this] { run(); });
}

void ClientService::stop() {
    if (!running_.load()) {
        // Not running, but the thread may still be joinable after a prior stop.
        if (worker_.joinable()) worker_.join();
        return;
    }
    stop_.store(true);
    if (worker_.joinable()) worker_.join();
    running_.store(false);
    setState(ServiceState::Stopped);
}

ServiceState ClientService::state() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

std::vector<double> ClientService::rttSamplesMs() const {
    // receiver_ is only touched by the worker thread while running; callers read this
    // after stop() (CLI --latency-report) or accept a benign racing snapshot.
    return receiver_.rttSamplesMs();
}

void ClientService::setState(ServiceState s) {
    StateCallback cb;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ == s) return;  // only fire on real transitions
        state_ = s;
        cb = cb_;
    }
    if (cb) cb(s);  // invoke outside the lock so callbacks can't deadlock on us
}

void ClientService::run() {
    if (!adb_.ready()) {
        // adb is missing: nothing to poll. Sit idle until stop() (re-discovery would
        // need a restart anyway). No busy-wait.
        setState(ServiceState::AdbMissing);
        while (!stop_.load()) sleepResponsive(cfg_.devicePollMs, stop_);
        return;
    }

    std::string forwardedSerial;          // device the forward is currently set for
    int idleBackoff = cfg_.devicePollMs;  // grows toward devicePollMaxMs while idle

    while (!stop_.load()) {
        const DeviceQuery q = adb_.queryDevices();

        if (q.status != DeviceStatus::Ready) {
            forwardedSerial.clear();
            setState(mapNonReady(q.status));
            sleepResponsive(idleBackoff, stop_);
            idleBackoff = std::min(idleBackoff + cfg_.devicePollMs, cfg_.devicePollMaxMs);
            continue;
        }
        idleBackoff = cfg_.devicePollMs;  // a device is ready: reset the backoff

        // Set up the adb forward once per device (idempotent; no app auto-launch).
        if (forwardedSerial != q.serial) {
            if (!adb_.setupForward(q.serial)) {
                setState(ServiceState::ForwardFailed);
                forwardedSerial.clear();
                sleepResponsive(cfg_.devicePollMs, stop_);
                continue;
            }
            forwardedSerial = q.serial;
        }

        // Armed: device ready + forwarded. Probe the forwarded socket; with adb
        // forward, connect() succeeds even before the app starts (the forward accepts
        // then closes with no data), so `gotData` tells a real session from that idle
        // case. Connected is flipped the moment the first byte arrives (onFirstData).
        setState(ServiceState::WaitingForApp);
        bool gotData = false;
        runClientConnection(receiver_, cfg_.port, stop_, 1000, &gotData,
                            [this] { setState(ServiceState::Connected); });

        if (!gotData && !stop_.load()) {
            // App not started yet: wait the conservative app-poll interval before the
            // next probe instead of spinning.
            sleepResponsive(cfg_.appPollMs, stop_);
        }
        // gotData: the session ended (disconnect already emitted a lift). Loop back —
        // re-query devices and return to WaitingForApp/idle as appropriate.
    }
}

}  // namespace phone2pad::client
