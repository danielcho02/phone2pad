// Trace playback (docs/05 §4): drive a Sink with timed frames, preserving the
// original inter-frame gaps. Factored out of the replay tool so unit tests can run
// it with a MockInputInjector and a no-op sleep (phone-free L3 testing).
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "phantompad/client/sink.hpp"
#include "phantompad/client/trace.hpp"

namespace phantompad::client {

// Sleep callback: receives a duration in microseconds. Tests pass a no-op.
using SleepFn = std::function<void(std::uint32_t)>;

// Real wall-clock sleep (used by the replay tool).
void realSleepMicros(std::uint32_t micros);

// Replay `frames` into `sink`. Gaps between consecutive frames are scaled by
// 1/speed (speed > 1 = faster). speed <= 0 plays with no delay.
void replayFrames(const std::vector<TimedFrame>& frames, Sink& sink, double speed,
                  const SleepFn& sleepMicros);

}  // namespace phantompad::client
