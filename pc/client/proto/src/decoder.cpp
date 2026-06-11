#include "phone2pad/proto/decoder.hpp"

#include "phone2pad/proto/packet.hpp"

namespace phone2pad::proto {
namespace {

std::uint16_t read_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Whether `length` is the exact valid payload size for `type` (docs/02 §7.3 rule 2).
bool length_ok(PacketType type, std::uint16_t length) {
    switch (type) {
        case PacketType::Hello:
            return length == kHelloPayloadSize;
        case PacketType::Ping:
        case PacketType::Pong:
            return length == kPingPongPayloadSize;
        case PacketType::Haptic:
            return length == kHapticPayloadSize;
        case PacketType::TouchFrame:
            return length >= kTouchFrameHeaderSize &&
                   (length - kTouchFrameHeaderSize) % kContactSize == 0 &&
                   (length - kTouchFrameHeaderSize) / kContactSize <= kMaxContacts;
    }
    return false;
}

// Decode a fully-present payload. `payload` points at the byte after the header.
DecodedPacket decode_payload(PacketType type, const std::uint8_t* payload, std::uint16_t length) {
    switch (type) {
        case PacketType::Hello: {
            Hello h;
            h.protocolVersion = payload[0];
            h.screenWidthPx = read_u16(payload + 1);
            h.screenHeightPx = read_u16(payload + 3);
            h.xdpiX10 = read_u16(payload + 5);
            h.ydpiX10 = read_u16(payload + 7);
            h.rotation = payload[9];
            h.maxContacts = payload[10];
            return h;
        }
        case PacketType::TouchFrame: {
            TouchFrame f;
            f.timestampUs = read_u32(payload);
            std::uint8_t count = payload[4];
            const std::uint8_t* c = payload + kTouchFrameHeaderSize;
            f.contacts.reserve(count);
            for (std::uint8_t i = 0; i < count; ++i, c += kContactSize) {
                Contact contact;
                contact.id = c[0];
                contact.tip = (c[1] & kFlagTip) != 0;
                contact.confidence = (c[1] & kFlagConfidence) != 0;
                contact.x = read_u16(c + 2);
                contact.y = read_u16(c + 4);
                f.contacts.push_back(contact);
            }
            (void)length;
            return f;
        }
        case PacketType::Ping:
            return PingPacket{PingPong{read_u32(payload), read_u32(payload + 4)}};
        case PacketType::Pong:
            return PongPacket{PingPong{read_u32(payload), read_u32(payload + 4)}};
        case PacketType::Haptic:
            return Haptic{payload[0]};
    }
    // Unreachable: callers validate type before calling.
    return Haptic{};
}

}  // namespace

std::vector<DecodedPacket> StreamDecoder::feed(std::span<const std::uint8_t> data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    std::vector<DecodedPacket> out;

    for (;;) {
        const std::size_t avail = buffer_.size() - cursor_;
        if (avail == 0) break;

        const std::uint8_t* p = buffer_.data() + cursor_;

        // Rule 1: drop bytes until the buffer head is the magic byte.
        if (p[0] != kMagic) {
            ++cursor_;
            ++skipped_;
            continue;
        }

        // Rule 5: wait for a complete header before validating.
        if (avail < kHeaderSize) break;

        const std::uint8_t type_byte = p[1];
        const std::uint16_t length = read_u16(p + 2);

        // Rule 2: reject invalid type/length, dropping only the leading byte so a
        // genuine magic later in the garbage is still found.
        if (!is_valid_type(type_byte) ||
            !length_ok(static_cast<PacketType>(type_byte), length)) {
            ++cursor_;
            ++skipped_;
            continue;
        }

        // Rule 5: wait for the full payload.
        if (avail < kHeaderSize + length) break;

        const auto type = static_cast<PacketType>(type_byte);
        const std::uint8_t* payload = p + kHeaderSize;

        // Rule 3: content validation (TOUCH_FRAME contactCount vs length).
        if (type == PacketType::TouchFrame) {
            const std::uint8_t count = payload[4];
            if (count != (length - kTouchFrameHeaderSize) / kContactSize) {
                ++cursor_;
                ++skipped_;
                continue;
            }
        }

        out.push_back(decode_payload(type, payload, length));
        cursor_ += kHeaderSize + length;
    }

    compact();
    return out;
}

// Drop already-consumed bytes so the buffer doesn't grow unbounded.
void StreamDecoder::compact() {
    if (cursor_ == 0) return;
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(cursor_));
    cursor_ = 0;
}

}  // namespace phone2pad::proto
