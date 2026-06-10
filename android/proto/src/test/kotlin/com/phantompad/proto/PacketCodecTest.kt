// Round-trip + stream-resync tests driven by the shared proto/test-vectors.
// Mirrors the C++ proto_tests so both implementations validate the same data.
package com.phantompad.proto

import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.junit.jupiter.api.DynamicTest
import org.junit.jupiter.api.TestFactory

class PacketCodecTest {

    @TestFactory
    fun singlePacketRoundTrip(): List<DynamicTest> {
        val vectors = TestVectorLoader.load().filter { it.contains("packet_hex") }
        assertTrue(vectors.size >= 9, "expected at least 9 single-packet vectors")
        return vectors.map { v ->
            val name = v["name"].string
            DynamicTest.dynamicTest("round-trip $name") {
                val hex = v["packet_hex"].string
                val decoded = v["decoded"].obj

                // encode(decoded) == hex
                assertEquals(hex, bytesToHex(encodeDecoded(decoded)), "encode mismatch for $name")

                // decode(hex) == decoded, exactly one packet, no skipped bytes
                val dec = StreamDecoder()
                val packets = dec.feed(hexToBytes(hex))
                assertEquals(1, packets.size, "expected one packet for $name")
                assertEquals(0, dec.skippedByteCount, "no skip expected for $name")
                assertDecodedMatches(packets[0], decoded, name)
            }
        }
    }

    @TestFactory
    fun singlePacketByteSplit(): List<DynamicTest> {
        val vectors = TestVectorLoader.load().filter { it.contains("packet_hex") }
        return vectors.map { v ->
            val name = v["name"].string
            DynamicTest.dynamicTest("byte-split $name") {
                val bytes = hexToBytes(v["packet_hex"].string)
                val dec = StreamDecoder()
                val all = ArrayList<DecodedPacket>()
                for (b in bytes) all.addAll(dec.feed(byteArrayOf(b)))
                assertEquals(1, all.size, "expected one packet for $name")
                assertEquals(0, dec.skippedByteCount, "no skip expected for $name")
                assertDecodedMatches(all[0], v["decoded"].obj, name)
            }
        }
    }

    @TestFactory
    fun streamResync(): List<DynamicTest> {
        val vectors = TestVectorLoader.load().filter { it.contains("stream_hex") }
        assertTrue(vectors.isNotEmpty(), "expected at least one resync vector")
        val tests = ArrayList<DynamicTest>()
        for (v in vectors) {
            val name = v["name"].string
            for (split in listOf(false, true)) {
                val label = if (split) "byte-split" else "whole"
                tests.add(DynamicTest.dynamicTest("resync $name ($label)") {
                    runResync(v, split)
                })
            }
        }
        return tests
    }

    private fun runResync(v: Json.Obj, byteSplit: Boolean) {
        val name = v["name"].string
        val stream = hexToBytes(v["stream_hex"].string)
        val expected = v["expected_packets"].array.map { it.obj }
        val expectedSkipped = v["skipped_bytes"].int

        val dec = StreamDecoder()
        val got = ArrayList<DecodedPacket>()
        if (byteSplit) {
            for (b in stream) got.addAll(dec.feed(byteArrayOf(b)))
        } else {
            got.addAll(dec.feed(stream))
        }

        assertEquals(expected.size, got.size, "packet count mismatch for $name")
        assertEquals(expectedSkipped, dec.skippedByteCount, "skip count mismatch for $name")
        for (idx in expected.indices) {
            assertDecodedMatches(got[idx], expected[idx], "$name[$idx]")
        }
    }

    // --- conversion helpers ---------------------------------------------------

    private fun encodeDecoded(d: Json.Obj): ByteArray = when (d["type"].string) {
        "HELLO" -> PacketEncoder.encode(
            Hello(
                protocolVersion = d["protocolVersion"].int,
                screenWidthPx = d["screenWidthPx"].int,
                screenHeightPx = d["screenHeightPx"].int,
                xdpiX10 = d["xdpiX10"].int,
                ydpiX10 = d["ydpiX10"].int,
                rotation = d["rotation"].int,
                maxContacts = d["maxContacts"].int,
            )
        )
        "TOUCH_FRAME" -> PacketEncoder.encode(
            TouchFrame(
                timestampUs = d["timestampUs"].long,
                contacts = d["contacts"].array.map { c ->
                    val o = c.obj
                    Contact(o["id"].int, o["tip"].bool, o["confidence"].bool, o["x"].int, o["y"].int)
                },
            )
        )
        "PING" -> PacketEncoder.encodePing(PingPong(d["seq"].long, d["senderTimestampUs"].long))
        "PONG" -> PacketEncoder.encodePong(PingPong(d["seq"].long, d["senderTimestampUs"].long))
        "HAPTIC" -> PacketEncoder.encode(Haptic(d["effect"].int))
        else -> error("unknown type ${d["type"].string}")
    }

    private fun assertDecodedMatches(pkt: DecodedPacket, d: Json.Obj, ctx: String) {
        when (d["type"].string) {
            "HELLO" -> {
                val h = (pkt as DecodedPacket.HelloPacket).value
                assertEquals(d["protocolVersion"].int, h.protocolVersion, "$ctx protocolVersion")
                assertEquals(d["screenWidthPx"].int, h.screenWidthPx, "$ctx screenWidthPx")
                assertEquals(d["screenHeightPx"].int, h.screenHeightPx, "$ctx screenHeightPx")
                assertEquals(d["xdpiX10"].int, h.xdpiX10, "$ctx xdpiX10")
                assertEquals(d["ydpiX10"].int, h.ydpiX10, "$ctx ydpiX10")
                assertEquals(d["rotation"].int, h.rotation, "$ctx rotation")
                assertEquals(d["maxContacts"].int, h.maxContacts, "$ctx maxContacts")
            }
            "TOUCH_FRAME" -> {
                val f = (pkt as DecodedPacket.TouchFramePacket).value
                assertEquals(d["timestampUs"].long, f.timestampUs, "$ctx timestampUs")
                val expected = d["contacts"].array.map { c ->
                    val o = c.obj
                    Contact(o["id"].int, o["tip"].bool, o["confidence"].bool, o["x"].int, o["y"].int)
                }
                assertEquals(expected, f.contacts, "$ctx contacts")
            }
            "PING" -> {
                val p = (pkt as DecodedPacket.PingPacket).value
                assertEquals(d["seq"].long, p.seq, "$ctx seq")
                assertEquals(d["senderTimestampUs"].long, p.senderTimestampUs, "$ctx senderTimestampUs")
            }
            "PONG" -> {
                val p = (pkt as DecodedPacket.PongPacket).value
                assertEquals(d["seq"].long, p.seq, "$ctx seq")
                assertEquals(d["senderTimestampUs"].long, p.senderTimestampUs, "$ctx senderTimestampUs")
            }
            "HAPTIC" -> {
                val h = (pkt as DecodedPacket.HapticPacket).value
                assertEquals(d["effect"].int, h.effect, "$ctx effect")
            }
            else -> error("unknown type ${d["type"].string}")
        }
    }
}
