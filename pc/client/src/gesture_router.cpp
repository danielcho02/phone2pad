#include "phone2pad/client/gesture_router.hpp"

namespace phone2pad::client {

namespace {

int activeCount(const proto::TouchFrame& frame) {
    int n = 0;
    for (const auto& c : frame.contacts) {
        if (c.tip) ++n;
    }
    return n;
}

}  // namespace

void GestureRouter::onFrame(const proto::TouchFrame& frame) {
    const int active = activeCount(frame);
    if (active >= 2) gestureSession_ = true;

    if (!gestureSession_) {
        one_.onFrame(frame);  // ordinary single-finger frame -> MouseSink
        return;
    }

    // First frame of a gesture session: let MouseSink see it once so it resets its
    // anchor (it freezes on any non-single-contact frame). After that it is withheld
    // for the rest of the session so the 2->1 finger decay tail never moves the cursor.
    if (!mouseReset_) {
        one_.onFrame(frame);
        mouseReset_ = true;
    }
    multi_.onFrame(frame);

    if (active == 0) {  // all fingers lifted -> session over
        gestureSession_ = false;
        mouseReset_ = false;
    }
}

}  // namespace phone2pad::client
