#include "phone2pad/proto/encoder.hpp"

#include "phone2pad/proto/packet.hpp"

namespace phone2pad::proto {
namespace {

void put_u8(Bytes& out, std::uint8_t v) { out.push_back(v); }

void put_u16(Bytes& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put_u32(Bytes& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

// Prepend the 4-byte common header (magic | type | length) to a payload.
Bytes frame(PacketType type, const Bytes& payload) {
    Bytes out;
    out.reserve(kHeaderSize + payload.size());
    put_u8(out, kMagic);
    put_u8(out, static_cast<std::uint8_t>(type));
    put_u16(out, static_cast<std::uint16_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

Bytes encode_ping_pong(const PingPong& pp, PacketType type) {
    Bytes payload;
    put_u32(payload, pp.seq);
    put_u32(payload, pp.senderTimestampUs);
    return frame(type, payload);
}

}  // namespace

Bytes encode(const Hello& hello) {
    Bytes payload;
    put_u8(payload, hello.protocolVersion);
    put_u16(payload, hello.screenWidthPx);
    put_u16(payload, hello.screenHeightPx);
    put_u16(payload, hello.xdpiX10);
    put_u16(payload, hello.ydpiX10);
    put_u8(payload, hello.rotation);
    put_u8(payload, hello.maxContacts);
    return frame(PacketType::Hello, payload);
}

Bytes encode(const TouchFrame& f) {
    Bytes payload;
    put_u32(payload, f.timestampUs);
    put_u8(payload, static_cast<std::uint8_t>(f.contacts.size()));
    for (const Contact& c : f.contacts) {
        put_u8(payload, c.id);
        std::uint8_t flags = 0;
        if (c.tip) flags |= kFlagTip;
        if (c.confidence) flags |= kFlagConfidence;
        put_u8(payload, flags);
        put_u16(payload, c.x);
        put_u16(payload, c.y);
    }
    return frame(PacketType::TouchFrame, payload);
}

Bytes encode_ping(const PingPong& ping) { return encode_ping_pong(ping, PacketType::Ping); }
Bytes encode_pong(const PingPong& pong) { return encode_ping_pong(pong, PacketType::Pong); }

Bytes encode(const Haptic& haptic) {
    Bytes payload;
    put_u8(payload, haptic.effect);
    return frame(PacketType::Haptic, payload);
}

}  // namespace phone2pad::proto
