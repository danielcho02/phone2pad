// FrameReceiver: turns a byte stream into Sink calls + latency stats. The socket
// loop and unit tests both drive it via processBytes(), so it is network-free and
// directly testable (docs/05 L2).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "phone2pad/client/sink.hpp"
#include "phone2pad/proto/decoder.hpp"
#include "phone2pad/proto/encoder.hpp"
#include "phone2pad/proto/hello.hpp"

namespace phone2pad::client {

class FrameReceiver {
public:
    // Monotonic clock in microseconds (u32, wrap allowed) — matches the PING stamp.
    using MicroClock = std::function<std::uint32_t()>;

    explicit FrameReceiver(Sink& sink, MicroClock clock = {});

    // Feed received bytes; dispatches each complete packet:
    //   TouchFrame -> sink.onFrame, Pong -> RTT sample, Hello -> stored.
    void processBytes(std::span<const std::uint8_t> data);

    // Emit one all-lift frame to the sink (call on disconnect; docs/01 §5).
    void emitLift();

    // Next PING packet: advances seq, stamps senderTimestampUs from the clock.
    proto::Bytes makePing();

    const std::vector<double>& rttSamplesMs() const { return rttMs_; }
    const std::optional<proto::Hello>& hello() const { return hello_; }
    std::size_t resyncSkippedBytes() const { return decoder_.skippedByteCount(); }

private:
    Sink& sink_;
    MicroClock clock_;
    proto::StreamDecoder decoder_;
    std::optional<proto::Hello> hello_;
    std::uint32_t pingSeq_ = 0;
    std::vector<double> rttMs_;
};

}  // namespace phone2pad::client
