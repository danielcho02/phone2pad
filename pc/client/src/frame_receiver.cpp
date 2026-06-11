#include "phone2pad/client/frame_receiver.hpp"

#include <chrono>
#include <variant>

#include "phone2pad/proto/ping_pong.hpp"
#include "phone2pad/proto/touch_frame.hpp"

namespace phone2pad::client {

namespace {

std::uint32_t steadyMicros() {
    using namespace std::chrono;
    const auto us = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    return static_cast<std::uint32_t>(us);
}

}  // namespace

FrameReceiver::FrameReceiver(Sink& sink, MicroClock clock)
    : sink_(sink), clock_(clock ? std::move(clock) : MicroClock(steadyMicros)) {}

void FrameReceiver::processBytes(std::span<const std::uint8_t> data) {
    for (auto& pkt : decoder_.feed(data)) {
        if (auto* frame = std::get_if<proto::TouchFrame>(&pkt)) {
            sink_.onFrame(*frame);
        } else if (auto* hello = std::get_if<proto::Hello>(&pkt)) {
            hello_ = *hello;
        } else if (auto* pong = std::get_if<proto::PongPacket>(&pkt)) {
            const std::uint32_t rttUs = clock_() - pong->value.senderTimestampUs;
            rttMs_.push_back(static_cast<double>(rttUs) / 1000.0);
        }
        // PingPacket/Haptic are not expected PC-side; ignored.
    }
}

void FrameReceiver::emitLift() {
    sink_.onFrame(proto::TouchFrame{});  // timestamp 0, no contacts = all lifted
}

proto::Bytes FrameReceiver::makePing() {
    proto::PingPong ping{};
    ping.seq = ++pingSeq_;
    ping.senderTimestampUs = clock_();
    return proto::encode_ping(ping);
}

}  // namespace phone2pad::client
