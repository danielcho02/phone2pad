// Orientation normalization (docs/01 §2.2, CLAUDE.md design #3/#4): the phone is a dumb
// sensor that streams raw absolute coords in its own Activity frame plus its display
// rotation (HELLO §3). Mapping the two landscape orientations onto a single canonical
// frame is the PC's job, so the same physical gesture feels identical whether the USB
// port is on the right (canonical) or, after a 180° flip, on the left.
//
// Canonical = landscape with the USB port on the right == Surface.ROTATION_90 (HELLO
// rotation value 1) for a portrait-natural phone. The opposite landscape,
// ROTATION_270 (value 3), is the same frame rotated 180°, so we flip both axes to
// normalize it. We deliberately do NOT detect the USB port physically; rotation is the
// only signal. If a particular device reports its canonical landscape as 270 instead of
// 90, the two orientations still stay mutually consistent (worst case is a harmless
// global flip), which is the property that matters most here.
#pragma once

#include "phone2pad/client/sink.hpp"
#include "phone2pad/proto/touch_frame.hpp"

namespace phone2pad::client {

// Normalize one frame to the canonical landscape. When rotation is the flipped landscape
// (ROTATION_270 == 3) and width/height are valid, every contact is rotated 180°
// (x -> w-1-x, y -> h-1-y). All other rotations (incl. 0/1/2) and invalid dims pass the
// frame through unchanged. id/tip/confidence and contact count are preserved.
proto::TouchFrame normalizeFrame(const proto::TouchFrame& frame, int rotation, int widthPx,
                                 int heightPx);

// Sink decorator that applies normalizeFrame() before forwarding to a downstream sink.
// Orientation is fed from HELLO (FrameReceiver::setHelloHandler). Defaults to the
// canonical/identity transform until the first HELLO arrives.
class OrientationNormalizingSink : public Sink {
public:
    explicit OrientationNormalizingSink(Sink& downstream) : down_(downstream) {}

    // rotation: HELLO rotation field (0/1/2/3). widthPx/heightPx: HELLO screen dims.
    void setOrientation(int rotation, int widthPx, int heightPx) {
        rotation_ = rotation;
        widthPx_ = widthPx;
        heightPx_ = heightPx;
    }

    void onFrame(const proto::TouchFrame& frame) override {
        down_.onFrame(normalizeFrame(frame, rotation_, widthPx_, heightPx_));
    }

private:
    Sink& down_;
    int rotation_ = 1;   // canonical (ROTATION_90) until HELLO says otherwise
    int widthPx_ = 0;
    int heightPx_ = 0;
};

}  // namespace phone2pad::client
