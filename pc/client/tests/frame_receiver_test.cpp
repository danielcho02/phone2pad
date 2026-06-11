// FrameReceiver unit tests (docs/05 L2): dispatch of decoded packets to the Sink,
// HELLO capture, PONG -> RTT, byte-split reassembly, and lift-on-disconnect.
#include <cstdint>
#include <span>
#include <vector>

#include "phone2pad/client/frame_receiver.hpp"
#include "phone2pad/client/sink.hpp"
#include "phone2pad/proto/encoder.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;
using phone2pad::proto::Bytes;
using phone2pad::proto::Contact;
using phone2pad::proto::Hello;
using phone2pad::proto::PingPong;
using phone2pad::proto::TouchFrame;

namespace {

struct CollectingSink : Sink {
    std::vector<TouchFrame> frames;
    void onFrame(const TouchFrame& f) override { frames.push_back(f); }
};

std::span<const std::uint8_t> sp(const Bytes& b) {
    return std::span<const std::uint8_t>(b.data(), b.size());
}

TouchFrame oneContactFrame() {
    TouchFrame f;
    f.timestampUs = 16666;
    Contact c;
    c.id = 0;
    c.tip = true;
    c.x = 540;
    c.y = 1200;
    f.contacts.push_back(c);
    return f;
}

}  // namespace

TEST_CASE("dispatches TOUCH_FRAME to the sink and stores HELLO") {
    CollectingSink sink;
    FrameReceiver rx(sink);

    Hello hello;
    hello.screenWidthPx = 2400;
    hello.screenHeightPx = 1080;
    hello.maxContacts = 10;
    const Bytes helloBytes = phone2pad::proto::encode(hello);
    rx.processBytes(sp(helloBytes));

    const Bytes frameBytes = phone2pad::proto::encode(oneContactFrame());
    rx.processBytes(sp(frameBytes));

    CHECK(rx.hello().has_value());
    if (rx.hello()) CHECK_EQ(rx.hello()->screenWidthPx, static_cast<std::uint16_t>(2400));
    CHECK_EQ(sink.frames.size(), static_cast<std::size_t>(1));
    if (!sink.frames.empty()) CHECK_EQ(sink.frames[0], oneContactFrame());
}

TEST_CASE("byte-split feed still yields one frame") {
    CollectingSink sink;
    FrameReceiver rx(sink);
    const Bytes bytes = phone2pad::proto::encode(oneContactFrame());
    for (std::uint8_t b : bytes) {
        const std::uint8_t one[1] = {b};
        rx.processBytes(std::span<const std::uint8_t>(one, 1));
    }
    CHECK_EQ(sink.frames.size(), static_cast<std::size_t>(1));
}

TEST_CASE("PONG produces an RTT sample using the injected clock") {
    CollectingSink sink;
    // Fixed clock at 5000us; PONG echoes senderTimestampUs=1000 -> rtt 4.0ms.
    FrameReceiver rx(sink, []() -> std::uint32_t { return 5000; });
    PingPong pp;
    pp.seq = 1;
    pp.senderTimestampUs = 1000;
    const Bytes pong = phone2pad::proto::encode_pong(pp);
    rx.processBytes(sp(pong));

    CHECK_EQ(rx.rttSamplesMs().size(), static_cast<std::size_t>(1));
    if (!rx.rttSamplesMs().empty()) {
        const double rtt = rx.rttSamplesMs()[0];
        CHECK(rtt > 3.9 && rtt < 4.1);
    }
}

TEST_CASE("emitLift sends one empty (all-lifted) frame") {
    CollectingSink sink;
    FrameReceiver rx(sink);
    rx.emitLift();
    CHECK_EQ(sink.frames.size(), static_cast<std::size_t>(1));
    if (!sink.frames.empty()) CHECK(sink.frames[0].contacts.empty());
}

TEST_CASE("makePing advances the sequence number") {
    CollectingSink sink;
    FrameReceiver rx(sink);
    const Bytes p1 = rx.makePing();
    const Bytes p2 = rx.makePing();
    // seq is the first 4 payload bytes (after the 4-byte header), little-endian.
    CHECK_EQ(p1[4], static_cast<std::uint8_t>(1));
    CHECK_EQ(p2[4], static_cast<std::uint8_t>(2));
}
