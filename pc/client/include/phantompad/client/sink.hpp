// Sink strategy interface (docs/01 §2.2). Phase transitions swap the Sink; the
// receive/parse path is shared across phases. Phase A uses MouseSink only.
#pragma once

#include "phantompad/proto/touch_frame.hpp"

namespace phantompad::client {

struct Sink {
    virtual ~Sink() = default;
    // Called on the receive thread for each decoded TouchFrame. Must not block.
    virtual void onFrame(const proto::TouchFrame& frame) = 0;
};

}  // namespace phantompad::client
