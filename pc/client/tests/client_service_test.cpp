// Unit tests for ClientService's front-end-facing surface. Kept pragmatic (per the
// v0.3.0 plan): the status-mapping helpers are pure and fully checkable, and the
// start/stop lifecycle is exercised for clean mechanics. We deliberately do NOT stand
// up a fake adb/socket/device — the worker's device branch is covered by AdbManager's
// own tests and by replay/trace tests.
#include "phone2pad/client/client_service.hpp"

#include <string>

#include "test_main.hpp"

using namespace phone2pad::client;

namespace {

// All states, so the mapping tests stay exhaustive as the enum grows.
constexpr ServiceState kAllStates[] = {
    ServiceState::Stopped,         ServiceState::AdbMissing,
    ServiceState::NoDevice,        ServiceState::Unauthorized,
    ServiceState::DeviceOffline,   ServiceState::MultipleDevices,
    ServiceState::ForwardFailed,   ServiceState::WaitingForApp,
    ServiceState::Connected,
};

}  // namespace

TEST_CASE("shortLabel maps every state into the four user-facing buckets") {
    // Idle (nothing to do)
    CHECK(std::string(shortLabel(ServiceState::Stopped)) == "Idle");
    CHECK(std::string(shortLabel(ServiceState::NoDevice)) == "Idle");
    // adb setup problem surfaced distinctly
    CHECK(std::string(shortLabel(ServiceState::AdbMissing)) == "adb not found");
    // device seen but not usable
    CHECK(std::string(shortLabel(ServiceState::Unauthorized)) == "Waiting for phone");
    CHECK(std::string(shortLabel(ServiceState::DeviceOffline)) == "Waiting for phone");
    CHECK(std::string(shortLabel(ServiceState::MultipleDevices)) == "Waiting for phone");
    CHECK(std::string(shortLabel(ServiceState::ForwardFailed)) == "Waiting for phone");
    // armed / active
    CHECK(std::string(shortLabel(ServiceState::WaitingForApp)) == "Waiting for app");
    CHECK(std::string(shortLabel(ServiceState::Connected)) == "Connected");
}

TEST_CASE("describe returns non-empty text for every state") {
    for (const ServiceState s : kAllStates) {
        CHECK(describe(s)[0] != '\0');
    }
}

TEST_CASE("ClientService starts in Stopped and stop() is safe before start()") {
    ClientService service;
    CHECK(!service.running());
    CHECK(service.state() == ServiceState::Stopped);
    service.stop();  // idempotent no-op before start
    CHECK(!service.running());
}

TEST_CASE("ClientService start/stop threading is clean (adb-missing idle path)") {
    // The start/stop threading is exercised only when adb is absent: that path runs a
    // pure idle sleep loop with NO device interaction. When adb is installed (typical
    // dev/CI machines) starting the worker would poll — and possibly set a forward /
    // connect to — a real, possibly-connected device, so we skip the spin here to keep
    // the test deterministic and side-effect-free. Full worker behavior with a device
    // is covered by manual L4 verification.
    ClientService service;
    if (service.adbReady()) {
        return;  // adb present: don't touch a real device from a unit test
    }
    service.start();
    CHECK(service.running());
    service.start();  // idempotent: second start is a no-op
    CHECK(service.running());

    service.stop();
    CHECK(!service.running());
    CHECK(service.state() == ServiceState::Stopped);
    service.stop();  // idempotent: second stop is a no-op
    CHECK(!service.running());
}
