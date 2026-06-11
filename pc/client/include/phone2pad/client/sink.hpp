// Sink strategy interface (docs/01 §2.2). Phase transitions swap the Sink; the
// receive/parse path is shared across phases. Phase A uses MouseSink only.
#pragma once

#include "phone2pad/proto/touch_frame.hpp"

namespace phone2pad::client {

struct Sink {
    virtual ~Sink() = default;
    // Called on the receive thread for each decoded TouchFrame. Must not block.
    virtual void onFrame(const proto::TouchFrame& frame) = 0;
};

}  // namespace phone2pad::client
