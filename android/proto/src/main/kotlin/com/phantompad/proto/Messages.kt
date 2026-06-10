// Decoded packet payloads. Field layout mirrors the C++ structs 1:1 so both
// implementations agree on the shared test vectors. See docs/02-PROTOCOL.md.
package com.phantompad.proto

/** HELLO (0x01) — §3. */
data class Hello(
    val protocolVersion: Int = 1,
    val screenWidthPx: Int,
    val screenHeightPx: Int,
    val xdpiX10: Int,
    val ydpiX10: Int,
    val rotation: Int,
    val maxContacts: Int,
)

/** A single touch contact within a TOUCH_FRAME — §4. */
data class Contact(
    val id: Int,
    val tip: Boolean,
    val confidence: Boolean,
    val x: Int,
    val y: Int,
)

/** TOUCH_FRAME (0x02) — §4. */
data class TouchFrame(
    val timestampUs: Long,            // u32 on the wire; Long avoids sign issues
    val contacts: List<Contact>,
)

/** PING (0x03) / PONG (0x04) shared payload — §5. */
data class PingPong(
    val seq: Long,                    // u32 on the wire
    val senderTimestampUs: Long,      // u32 on the wire
)

/** HAPTIC (0x05) — §6 (Phase D). */
data class Haptic(
    val effect: Int,
)

/** Tagged result of decoding one packet from the stream. */
sealed interface DecodedPacket {
    data class HelloPacket(val value: Hello) : DecodedPacket
    data class TouchFramePacket(val value: TouchFrame) : DecodedPacket
    data class PingPacket(val value: PingPong) : DecodedPacket
    data class PongPacket(val value: PingPong) : DecodedPacket
    data class HapticPacket(val value: Haptic) : DecodedPacket
}
