// OrientationNormalizer tests (docs/05 L2): the two landscape orientations must map onto
// one canonical frame so the same physical gesture is consistent after a 180° flip.
// normalizeFrame is identity for the canonical landscape (and unknown dims) and a 180°
// flip for the reverse landscape (ROTATION_270); the end-to-end case proves a reverse
// swipe scrolls the same direction as the canonical swipe.
#include <vector>

#include "mock_injector.hpp"
#include "phone2pad/client/gesture_sink.hpp"
#include "phone2pad/client/orientation_normalizer.hpp"
#include "test_main.hpp"

using namespace phone2pad::client;
using phone2pad::proto::Contact;
using phone2pad::proto::TouchFrame;

namespace {

constexpr int kW = 2400, kH = 1080;

Contact ct(std::uint8_t id, bool tip, bool conf, std::uint16_t x, std::uint16_t y) {
    Contact c;
    c.id = id;
    c.tip = tip;
    c.confidence = conf;
    c.x = x;
    c.y = y;
    return c;
}

TouchFrame frame(std::uint32_t t, std::vector<Contact> contacts) {
    TouchFrame f;
    f.timestampUs = t;
    f.contacts = std::move(contacts);
    return f;
}

void feed(Sink& s, const std::vector<TouchFrame>& frames) {
    for (const auto& f : frames) s.onFrame(f);
}

}  // namespace

TEST_CASE("normalizeFrame: canonical landscape (rotation 1) is identity") {
    const TouchFrame f = frame(7, {ct(0, true, true, 100, 200), ct(1, false, false, 300, 400)});
    CHECK(normalizeFrame(f, /*rotation=*/1, kW, kH) == f);
}

TEST_CASE("normalizeFrame: rotations 0 and 2 pass through unchanged") {
    const TouchFrame f = frame(7, {ct(0, true, true, 100, 200)});
    CHECK(normalizeFrame(f, /*rotation=*/0, kW, kH) == f);
    CHECK(normalizeFrame(f, /*rotation=*/2, kW, kH) == f);
}

TEST_CASE("normalizeFrame: invalid dims pass through unchanged even at rotation 3") {
    const TouchFrame f = frame(7, {ct(0, true, true, 100, 200)});
    CHECK(normalizeFrame(f, /*rotation=*/3, 0, kH) == f);
    CHECK(normalizeFrame(f, /*rotation=*/3, kW, 0) == f);
}

TEST_CASE("normalizeFrame: reverse landscape (rotation 3) flips both axes, keeps fields") {
    const TouchFrame f = frame(42, {ct(3, true, false, 100, 200), ct(5, false, true, 2399, 1079)});
    const TouchFrame n = normalizeFrame(f, /*rotation=*/3, kW, kH);
    CHECK_EQ(n.contacts.size(), std::size_t{2});
    CHECK_EQ(n.timestampUs, 42u);
    // (100,200) -> (2400-1-100, 1080-1-200) = (2299, 879)
    CHECK_EQ(int(n.contacts[0].x), 2299);
    CHECK_EQ(int(n.contacts[0].y), 879);
    CHECK_EQ(int(n.contacts[0].id), 3);
    CHECK(n.contacts[0].tip == true);
    CHECK(n.contacts[0].confidence == false);
    // (2399,1079) -> (0,0)
    CHECK_EQ(int(n.contacts[1].x), 0);
    CHECK_EQ(int(n.contacts[1].y), 0);
    CHECK(n.contacts[1].tip == false);
    CHECK(n.contacts[1].confidence == true);
}

TEST_CASE("OrientationNormalizingSink default is canonical/identity until HELLO") {
    MockInjector inj;
    GestureSink gesture(inj);
    OrientationNormalizingSink norm(gesture);  // no setOrientation() yet
    // Canonical swipe up: should scroll (natural -> negative wheel) just like a raw frame.
    feed(norm, {
        frame(0, {ct(0, true, true, 200, 300), ct(1, true, true, 400, 300)}),
        frame(100000, {ct(0, true, true, 200, 300), ct(1, true, true, 400, 300)}),  // settle
        frame(150000, {ct(0, true, true, 200, 220), ct(1, true, true, 400, 220)}),
        frame(200000, {ct(0, true, true, 200, 140), ct(1, true, true, 400, 140)}),
        frame(250000, {ct(0, false, true, 200, 140), ct(1, false, true, 400, 140)}),
    });
    CHECK(inj.wheelSum < 0);  // identity transform: natural swipe-up scrolls to lower content
}

TEST_CASE("reverse-landscape swipe normalizes to the same direction as canonical") {
    // Same physical swipe-up performed in each landscape. In the reverse landscape the
    // device-frame coords are the canonical ones rotated 180° (x->W-1-x, y->H-1-y), so the
    // raw motion reads as a downward swipe. After normalization both must scroll the same
    // way (negative wheel), which is the whole point: gesture feel is orientation-stable.
    const std::vector<TouchFrame> canonical = {
        frame(0, {ct(0, true, true, 200, 300), ct(1, true, true, 400, 300)}),
        frame(100000, {ct(0, true, true, 200, 300), ct(1, true, true, 400, 300)}),
        frame(150000, {ct(0, true, true, 200, 220), ct(1, true, true, 400, 220)}),
        frame(200000, {ct(0, true, true, 200, 140), ct(1, true, true, 400, 140)}),
        frame(250000, {ct(0, false, true, 200, 140), ct(1, false, true, 400, 140)}),
    };
    // Reverse = 180° flip of every coord for the kW x kH frame.
    std::vector<TouchFrame> reverse;
    for (const TouchFrame& f : canonical) {
        std::vector<Contact> cs;
        for (const Contact& c : f.contacts) {
            cs.push_back(ct(c.id, c.tip, c.confidence,
                            static_cast<std::uint16_t>(kW - 1 - c.x),
                            static_cast<std::uint16_t>(kH - 1 - c.y)));
        }
        reverse.push_back(frame(f.timestampUs, std::move(cs)));
    }

    MockInjector canonInj;
    GestureSink canonGesture(canonInj);
    OrientationNormalizingSink canonNorm(canonGesture);
    canonNorm.setOrientation(/*rotation=*/1, kW, kH);  // canonical: identity
    feed(canonNorm, canonical);

    MockInjector revInj;
    GestureSink revGesture(revInj);
    OrientationNormalizingSink revNorm(revGesture);
    revNorm.setOrientation(/*rotation=*/3, kW, kH);  // reverse: 180° flip back to canonical
    feed(revNorm, reverse);

    CHECK(canonInj.wheelSum < 0);   // canonical natural swipe-up
    CHECK(revInj.wheelSum < 0);     // reverse swipe normalizes to the same direction
    CHECK_EQ(canonInj.wheelSum, revInj.wheelSum);  // identical, not merely same sign
}
