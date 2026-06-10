// Stream decoder: length-prefixed packet stream -> decoded packets, with
// resynchronization on corruption. Rules: docs/02-PROTOCOL.md §7.3.
// Must stay byte-for-byte equivalent to the C++ StreamDecoder.
package com.phantompad.proto

/**
 * Accumulates bytes across [feed] calls and emits packets as they complete.
 * Bytes skipped during resynchronization are counted in [skippedByteCount].
 */
class StreamDecoder {
    private var buffer = ByteArray(0)
    private var cursor = 0            // index of next unconsumed byte
    var skippedByteCount: Int = 0
        private set

    val bufferedByteCount: Int get() = buffer.size - cursor

    /** Append [data] and return any packets that became complete. */
    fun feed(data: ByteArray): List<DecodedPacket> {
        buffer = buffer.copyOf(buffer.size + data.size).also {
            System.arraycopy(data, 0, it, buffer.size, data.size)
        }
        val out = ArrayList<DecodedPacket>()

        loop@ while (true) {
            val avail = buffer.size - cursor
            if (avail == 0) break

            val base = cursor

            // Rule 1: drop bytes until the buffer head is the magic byte.
            if (u8(base) != MAGIC) {
                cursor++; skippedByteCount++; continue
            }

            // Rule 5: wait for a complete header.
            if (avail < HEADER_SIZE) break

            val typeByte = u8(base + 1)
            val length = u8(base + 2) or (u8(base + 3) shl 8)
            val type = PacketType.fromValue(typeByte)

            // Rule 2: reject invalid type/length, dropping only the leading byte.
            if (type == null || !lengthOk(type, length)) {
                cursor++; skippedByteCount++; continue
            }

            // Rule 5: wait for the full payload.
            if (avail < HEADER_SIZE + length) break

            val payload = base + HEADER_SIZE

            // Rule 3: content validation (TOUCH_FRAME contactCount vs length).
            if (type == PacketType.TOUCH_FRAME) {
                val count = u8(payload + 4)
                if (count != (length - TOUCH_FRAME_HEADER_SIZE) / CONTACT_SIZE) {
                    cursor++; skippedByteCount++; continue
                }
            }

            out.add(decode(type, payload, length))
            cursor += HEADER_SIZE + length
        }

        compact()
        return out
    }

    private fun decode(type: PacketType, p: Int, length: Int): DecodedPacket = when (type) {
        PacketType.HELLO -> DecodedPacket.HelloPacket(
            Hello(
                protocolVersion = u8(p),
                screenWidthPx = u16(p + 1),
                screenHeightPx = u16(p + 3),
                xdpiX10 = u16(p + 5),
                ydpiX10 = u16(p + 7),
                rotation = u8(p + 9),
                maxContacts = u8(p + 10),
            )
        )
        PacketType.TOUCH_FRAME -> {
            val count = u8(p + 4)
            val contacts = ArrayList<Contact>(count)
            var c = p + TOUCH_FRAME_HEADER_SIZE
            repeat(count) {
                val flags = u8(c + 1)
                contacts.add(
                    Contact(
                        id = u8(c),
                        tip = (flags and FLAG_TIP) != 0,
                        confidence = (flags and FLAG_CONFIDENCE) != 0,
                        x = u16(c + 2),
                        y = u16(c + 4),
                    )
                )
                c += CONTACT_SIZE
            }
            DecodedPacket.TouchFramePacket(TouchFrame(u32(p), contacts))
        }
        PacketType.PING -> DecodedPacket.PingPacket(PingPong(u32(p), u32(p + 4)))
        PacketType.PONG -> DecodedPacket.PongPacket(PingPong(u32(p), u32(p + 4)))
        PacketType.HAPTIC -> DecodedPacket.HapticPacket(Haptic(u8(p)))
    }

    private fun lengthOk(type: PacketType, length: Int): Boolean = when (type) {
        PacketType.HELLO -> length == HELLO_PAYLOAD_SIZE
        PacketType.PING, PacketType.PONG -> length == PING_PONG_PAYLOAD_SIZE
        PacketType.HAPTIC -> length == HAPTIC_PAYLOAD_SIZE
        PacketType.TOUCH_FRAME ->
            length >= TOUCH_FRAME_HEADER_SIZE &&
                (length - TOUCH_FRAME_HEADER_SIZE) % CONTACT_SIZE == 0 &&
                (length - TOUCH_FRAME_HEADER_SIZE) / CONTACT_SIZE <= MAX_CONTACTS
    }

    /** Drop already-consumed bytes so the buffer doesn't grow unbounded. */
    private fun compact() {
        if (cursor == 0) return
        buffer = buffer.copyOfRange(cursor, buffer.size)
        cursor = 0
    }

    private fun u8(i: Int): Int = buffer[i].toInt() and 0xFF
    private fun u16(i: Int): Int = u8(i) or (u8(i + 1) shl 8)
    private fun u32(i: Int): Long =
        (u8(i).toLong()) or (u8(i + 1).toLong() shl 8) or
            (u8(i + 2).toLong() shl 16) or (u8(i + 3).toLong() shl 24)
}
