#include "phantompad/client/replay.hpp"

#include <chrono>
#include <thread>

namespace phantompad::client {

void realSleepMicros(std::uint32_t micros) {
    if (micros != 0) std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

void replayFrames(const std::vector<TimedFrame>& frames, Sink& sink, double speed,
                  const SleepFn& sleepMicros) {
    std::uint32_t prevT = 0;
    bool first = true;
    for (const TimedFrame& tf : frames) {
        if (!first) {
            const std::uint32_t gap = tf.t - prevT;  // u32 wrap-safe; monotonic traces
            if (speed > 0.0 && sleepMicros) {
                sleepMicros(static_cast<std::uint32_t>(gap / speed));
            }
        }
        sink.onFrame(tf.frame);
        prevT = tf.t;
        first = false;
    }
}

}  // namespace phantompad::client
