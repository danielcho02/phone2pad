#!/usr/bin/env python3
"""Reference encoder + test-vector generator for the phone2pad wire protocol.

Single source of truth: docs/02-PROTOCOL.md. This script is an INDEPENDENT third
implementation (besides C++ and Kotlin) used only to generate the shared test
vectors in proto/test-vectors/*.json. Output is deterministic: re-running this
script must produce byte-identical files (so they can be committed and diffed).

Wire format (little-endian), see docs/02-PROTOCOL.md:
  header (4 bytes): magic=0xA7 | type(u8) | length(u16, payload length)
  HELLO(0x01):       protocolVersion u8, screenWidthPx u16, screenHeightPx u16,
                     xdpiX10 u16, ydpiX10 u16, rotation u8, maxContacts u8   (11)
  TOUCH_FRAME(0x02): timestampUs u32, contactCount u8,
                     contacts[n] = { contactId u8, flags u8, x u16, y u16 }   (5+6n)
                     flags bit0=tip, bit1=confidence
  PING(0x03)/PONG(0x04): seq u32, senderTimestampUs u32                       (8)
  HAPTIC(0x05):      effect u8                                                (1)

Usage:
  python proto/tools/gen_test_vectors.py            # writes proto/test-vectors/*.json
  python proto/tools/gen_test_vectors.py --check     # verify on-disk files are current
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = 0xA7

TYPE_HELLO = 0x01
TYPE_TOUCH_FRAME = 0x02
TYPE_PING = 0x03
TYPE_PONG = 0x04
TYPE_HAPTIC = 0x05

FLAG_TIP = 0x01
FLAG_CONFIDENCE = 0x02


# --- low-level encoders -------------------------------------------------------

def _frame(packet_type: int, payload: bytes) -> bytes:
    """Prefix a payload with the 4-byte common header."""
    return struct.pack("<BBH", MAGIC, packet_type, len(payload)) + payload


def encode_hello(d: dict) -> bytes:
    payload = struct.pack(
        "<BHHHHBB",
        d["protocolVersion"],
        d["screenWidthPx"],
        d["screenHeightPx"],
        d["xdpiX10"],
        d["ydpiX10"],
        d["rotation"],
        d["maxContacts"],
    )
    return _frame(TYPE_HELLO, payload)


def encode_touch_frame(d: dict) -> bytes:
    contacts = d["contacts"]
    payload = struct.pack("<IB", d["timestampUs"] & 0xFFFFFFFF, len(contacts))
    for c in contacts:
        flags = (FLAG_TIP if c["tip"] else 0) | (FLAG_CONFIDENCE if c["confidence"] else 0)
        payload += struct.pack("<BBHH", c["id"], flags, c["x"], c["y"])
    return _frame(TYPE_TOUCH_FRAME, payload)


def encode_ping_pong(d: dict, packet_type: int) -> bytes:
    payload = struct.pack("<II", d["seq"], d["senderTimestampUs"])
    return _frame(packet_type, payload)


def encode_haptic(d: dict) -> bytes:
    return _frame(TYPE_HAPTIC, struct.pack("<B", d["effect"]))


def encode_decoded(decoded: dict) -> bytes:
    """Dispatch on decoded["type"] to produce the packet bytes."""
    t = decoded["type"]
    if t == "HELLO":
        return encode_hello(decoded)
    if t == "TOUCH_FRAME":
        return encode_touch_frame(decoded)
    if t == "PING":
        return encode_ping_pong(decoded, TYPE_PING)
    if t == "PONG":
        return encode_ping_pong(decoded, TYPE_PONG)
    if t == "HAPTIC":
        return encode_haptic(decoded)
    raise ValueError(f"unknown packet type: {t!r}")


# --- vector definitions -------------------------------------------------------

def _packet_vector(name: str, description: str, decoded: dict) -> dict:
    """Single-packet vector with packet_hex + decoded (docs/02 §7)."""
    return {
        "name": name,
        "description": description,
        "packet_hex": encode_decoded(decoded).hex(),
        "decoded": decoded,
    }


def build_single_packet_vectors() -> list[dict]:
    vectors: list[dict] = []

    vectors.append(_packet_vector(
        "hello_basic",
        "HELLO at connect: 1080x2400, 421.7/421.7 dpi, portrait, 10 contacts",
        {
            "type": "HELLO",
            "protocolVersion": 1,
            "screenWidthPx": 1080,
            "screenHeightPx": 2400,
            "xdpiX10": 4217,
            "ydpiX10": 4217,
            "rotation": 0,
            "maxContacts": 10,
        },
    ))

    vectors.append(_packet_vector(
        "touch_frame_empty",
        "Empty frame (contactCount=0): all fingers lifted",
        {"type": "TOUCH_FRAME", "timestampUs": 1000, "contacts": []},
    ))

    vectors.append(_packet_vector(
        "touch_frame_1contact",
        "Single contact down",
        {
            "type": "TOUCH_FRAME",
            "timestampUs": 16666,
            "contacts": [
                {"id": 0, "tip": True, "confidence": True, "x": 540, "y": 1200},
            ],
        },
    ))

    vectors.append(_packet_vector(
        "touch_frame_2contacts",
        "Two contacts, one with confidence=0 (palm-suspect)",
        {
            "type": "TOUCH_FRAME",
            "timestampUs": 305419896,
            "contacts": [
                {"id": 1, "tip": True, "confidence": False, "x": 1000, "y": 1055},
                {"id": 2, "tip": False, "confidence": True, "x": 1348, "y": 1630},
            ],
        },
    ))

    vectors.append(_packet_vector(
        "touch_frame_5contacts",
        "Five simultaneous contacts (full-hand gesture)",
        {
            "type": "TOUCH_FRAME",
            "timestampUs": 50000,
            "contacts": [
                {"id": 0, "tip": True, "confidence": True, "x": 100, "y": 200},
                {"id": 1, "tip": True, "confidence": True, "x": 300, "y": 400},
                {"id": 2, "tip": True, "confidence": True, "x": 500, "y": 600},
                {"id": 3, "tip": True, "confidence": True, "x": 700, "y": 800},
                {"id": 4, "tip": True, "confidence": True, "x": 900, "y": 1000},
            ],
        },
    ))

    vectors.append(_packet_vector(
        "touch_frame_lift",
        "Lift frame: contact reported once with tip=0 before removal",
        {
            "type": "TOUCH_FRAME",
            "timestampUs": 33333,
            "contacts": [
                {"id": 0, "tip": False, "confidence": True, "x": 540, "y": 1200},
            ],
        },
    ))

    vectors.append(_packet_vector(
        "ping",
        "PING from PC",
        {"type": "PING", "seq": 1, "senderTimestampUs": 123456},
    ))

    vectors.append(_packet_vector(
        "pong",
        "PONG echo from phone",
        {"type": "PONG", "seq": 1, "senderTimestampUs": 123456},
    ))

    vectors.append(_packet_vector(
        "haptic",
        "HAPTIC click effect (Phase D)",
        {"type": "HAPTIC", "effect": 0},
    ))

    return vectors


def build_resync_vector() -> dict:
    """Stream vector (docs/02 §7.2): garbage around two valid packets.

    skipped_bytes is computed mechanically by emulating the decoder rules in
    docs/02 §7.3 against this exact stream, so the expected value can never
    drift from the bytes we emit.
    """
    ping = encode_decoded({"type": "PING", "seq": 1, "senderTimestampUs": 123456})
    haptic = encode_decoded({"type": "HAPTIC", "effect": 1})

    # Leading garbage contains a decoy 0xA7 followed by an invalid type (0xFF),
    # which must be rejected and resynced past one byte at a time.
    leading = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0xA7, 0xFF, 0x00])
    middle = bytes([0x00, 0x11, 0x22])  # garbage between the two valid packets
    trailing = bytes([0x99])            # trailing partial garbage (no 0xA7)

    stream = leading + ping + middle + haptic + trailing
    expected = [
        {"type": "PING", "seq": 1, "senderTimestampUs": 123456},
        {"type": "HAPTIC", "effect": 1},
    ]
    skipped = _emulate_skipped(stream)

    return {
        "name": "resync_after_garbage",
        "description": "Recover two valid packets surrounded by garbage incl. a decoy 0xA7",
        "stream_hex": stream.hex(),
        "expected_packets": expected,
        "skipped_bytes": skipped,
    }


_VALID_TYPES = {TYPE_HELLO, TYPE_TOUCH_FRAME, TYPE_PING, TYPE_PONG, TYPE_HAPTIC}


def _length_ok(packet_type: int, length: int) -> bool:
    if packet_type == TYPE_HELLO:
        return length == 11
    if packet_type in (TYPE_PING, TYPE_PONG):
        return length == 8
    if packet_type == TYPE_HAPTIC:
        return length == 1
    if packet_type == TYPE_TOUCH_FRAME:
        return length >= 5 and (length - 5) % 6 == 0 and (length - 5) // 6 <= 16
    return False


def _emulate_skipped(stream: bytes) -> int:
    """Reference implementation of docs/02 §7.3 resync, returns skipped byte count.

    Mirrors the C++/Kotlin StreamDecoder so the committed vector stays in lockstep
    with the documented rules. Content-level validation here covers TOUCH_FRAME's
    contactCount == (length-5)/6 (always true by construction for our vectors).
    """
    buf = stream
    i = 0
    skipped = 0
    n = len(buf)
    while i < n:
        if buf[i] != MAGIC:
            skipped += 1
            i += 1
            continue
        if n - i < 4:
            break  # header not complete: wait for more (end of stream here)
        ptype = buf[i + 1]
        length = buf[i + 2] | (buf[i + 3] << 8)
        if ptype not in _VALID_TYPES or not _length_ok(ptype, length):
            skipped += 1
            i += 1
            continue
        if n - i < 4 + length:
            break  # payload not complete: wait for more
        # Content validation for TOUCH_FRAME.
        if ptype == TYPE_TOUCH_FRAME:
            contact_count = buf[i + 4]
            if contact_count != (length - 5) // 6:
                skipped += 1
                i += 1
                continue
        # Valid packet consumed.
        i += 4 + length
    return skipped


# --- main ---------------------------------------------------------------------

def all_vectors() -> dict[str, dict]:
    out: dict[str, dict] = {}
    for v in build_single_packet_vectors():
        out[v["name"]] = v
    rv = build_resync_vector()
    out[rv["name"]] = rv
    return out


def render(vector: dict) -> str:
    return json.dumps(vector, indent=2, ensure_ascii=False) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify on-disk vectors match generated output (exit 1 if not)")
    parser.add_argument("--out-dir", default=None,
                        help="output directory (default: <repo>/proto/test-vectors)")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    out_dir = Path(args.out_dir) if args.out_dir else repo_root / "proto" / "test-vectors"
    out_dir.mkdir(parents=True, exist_ok=True)

    vectors = all_vectors()

    if args.check:
        stale = []
        for name, vector in vectors.items():
            path = out_dir / f"{name}.json"
            expected = render(vector)
            if not path.exists() or path.read_text(encoding="utf-8") != expected:
                stale.append(name)
        if stale:
            print(f"[gen_test_vectors] STALE vectors: {', '.join(sorted(stale))}", file=sys.stderr)
            print("Re-run: python proto/tools/gen_test_vectors.py", file=sys.stderr)
            return 1
        print(f"[gen_test_vectors] {len(vectors)} vectors up to date.")
        return 0

    for name, vector in vectors.items():
        path = out_dir / f"{name}.json"
        path.write_text(render(vector), encoding="utf-8")
        print(f"[gen_test_vectors] wrote {path.relative_to(repo_root)}")
    print(f"[gen_test_vectors] {len(vectors)} vectors generated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
