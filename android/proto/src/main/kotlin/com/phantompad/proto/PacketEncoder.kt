// Packet encoders: struct -> wire bytes (little-endian). See docs/02-PROTOCOL.md.
package com.phantompad.proto

import java.nio.ByteBuffer
import java.nio.ByteOrder

object PacketEncoder {

    fun encode(hello: Hello): ByteArray {
        val payload = buffer(HELLO_PAYLOAD_SIZE).apply {
            put(hello.protocolVersion.toByte())
            putShort(hello.screenWidthPx.toShort())
            putShort(hello.screenHeightPx.toShort())
            putShort(hello.xdpiX10.toShort())
            putShort(hello.ydpiX10.toShort())
            put(hello.rotation.toByte())
            put(hello.maxContacts.toByte())
        }
        return frame(PacketType.HELLO, payload.array())
    }

    fun encode(frame: TouchFrame): ByteArray {
        val size = TOUCH_FRAME_HEADER_SIZE + frame.contacts.size * CONTACT_SIZE
        val payload = buffer(size).apply {
            putInt(frame.timestampUs.toInt())
            put(frame.contacts.size.toByte())
            for (c in frame.contacts) {
                put(c.id.toByte())
                var flags = 0
                if (c.tip) flags = flags or FLAG_TIP
                if (c.confidence) flags = flags or FLAG_CONFIDENCE
                put(flags.toByte())
                putShort(c.x.toShort())
                putShort(c.y.toShort())
            }
        }
        return frame(PacketType.TOUCH_FRAME, payload.array())
    }

    fun encodePing(ping: PingPong): ByteArray = encodePingPong(PacketType.PING, ping)

    fun encodePong(pong: PingPong): ByteArray = encodePingPong(PacketType.PONG, pong)

    fun encode(haptic: Haptic): ByteArray {
        val payload = buffer(HAPTIC_PAYLOAD_SIZE).apply { put(haptic.effect.toByte()) }
        return frame(PacketType.HAPTIC, payload.array())
    }

    private fun encodePingPong(type: PacketType, pp: PingPong): ByteArray {
        val payload = buffer(PING_PONG_PAYLOAD_SIZE).apply {
            putInt(pp.seq.toInt())
            putInt(pp.senderTimestampUs.toInt())
        }
        return frame(type, payload.array())
    }

    private fun buffer(size: Int): ByteBuffer =
        ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN)

    /** Prepend the 4-byte common header (magic | type | length) to a payload. */
    private fun frame(type: PacketType, payload: ByteArray): ByteArray {
        val out = buffer(HEADER_SIZE + payload.size)
        out.put(MAGIC.toByte())
        out.put(type.value.toByte())
        out.putShort(payload.size.toShort())
        out.put(payload)
        return out.array()
    }
}
