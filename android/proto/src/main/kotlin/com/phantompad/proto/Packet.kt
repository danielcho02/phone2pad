// phone2pad wire protocol — common header and packet-type definitions.
// Single source of truth: docs/02-PROTOCOL.md. Byte order is little-endian.
package com.phantompad.proto

/** Magic byte that starts every packet header. */
const val MAGIC: Int = 0xA7

/** Common header size in bytes: magic(1) + type(1) + length(2). */
const val HEADER_SIZE: Int = 4

/** Contact flag bits (TOUCH_FRAME contact.flags). */
const val FLAG_TIP: Int = 0x01          // 1 = touching, 0 = lifting
const val FLAG_CONFIDENCE: Int = 0x02   // 1 = normal, 0 = palm-suspect

/** Fixed payload sizes / bounds used for stream validation (docs/02 §7.3). */
const val HELLO_PAYLOAD_SIZE: Int = 11
const val PING_PONG_PAYLOAD_SIZE: Int = 8
const val HAPTIC_PAYLOAD_SIZE: Int = 1
const val TOUCH_FRAME_HEADER_SIZE: Int = 5   // timestampUs(4) + contactCount(1)
const val CONTACT_SIZE: Int = 6              // id(1)+flags(1)+x(2)+y(2)
const val MAX_CONTACTS: Int = 16

enum class PacketType(val value: Int) {
    HELLO(0x01),
    TOUCH_FRAME(0x02),
    PING(0x03),
    PONG(0x04),
    HAPTIC(0x05);

    companion object {
        fun fromValue(v: Int): PacketType? = entries.firstOrNull { it.value == v }
    }
}
