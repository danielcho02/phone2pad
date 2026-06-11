// GestureRouter (Phase B): top-level Sink that splits touch sessions by finger count
// (docs/01 §2.2). A session is single-finger until proven otherwise:
//   - 1-finger session  -> MouseSink (Phase A relative mouse, unchanged)
//   - any frame with 2+ active contacts latches the session to the GestureSink until
//     all fingers lift (peak-latch: never de-escalates mid-session).
// On entering a gesture session the first multi-touch frame is also forwarded once to
// the single-finger sink so its movement anchor resets (MouseSink freezes on != 1
// active contact), preventing a cursor jump when the gesture decays back to 1 finger.
#pragma once

#include "phone2pad/client/sink.hpp"

namespace phone2pad::client {

class GestureRouter : public Sink {
public:
    GestureRouter(Sink& oneFinger, Sink& multiFinger)
        : one_(oneFinger), multi_(multiFinger) {}

    void onFrame(const proto::TouchFrame& frame) override;

private:
    Sink& one_;
    Sink& multi_;
    bool gestureSession_ = false;  // latched while a 2+ finger session is in progress
    bool mouseReset_ = false;      // forwarded the anchor-reset frame to one_ this session
};

}  // namespace phone2pad::client
