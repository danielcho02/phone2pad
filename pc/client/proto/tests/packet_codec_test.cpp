// Round-trip tests for single-packet vectors (packet_hex + decoded). Emits main()
// for the whole proto_tests executable (PHANTOMPAD_TEST_MAIN).
#define PHANTOMPAD_TEST_MAIN
#include "test_main.hpp"

#include <span>
#include <string>

#include "json_lite.hpp"
#include "phantompad/proto/decoder.hpp"
#include "vector_support.hpp"

using namespace phantompad;
namespace ts = phantompad::testsupport;

TEST_CASE("single-packet vectors: decode(hex) == decoded, encode(decoded) == hex") {
    int checked = 0;
    for (const auto& path : ts::vector_files()) {
        const json::Value v = json::parse(ts::read_file(path));
        if (!v.contains("packet_hex")) continue;  // resync vectors handled elsewhere

        const std::string name = v.at("name").as_string();
        const ts::Bytes expected_bytes = ts::from_hex(v.at("packet_hex").as_string());
        const json::Value& decoded = v.at("decoded");

        // Encode direction: build struct from JSON, encode, compare hex.
        const ts::Bytes encoded = ts::encode_decoded(decoded);
        CHECK_EQ(ts::to_hex(encoded), v.at("packet_hex").as_string());

        // Decode direction: feed bytes, expect exactly one packet matching `decoded`.
        proto::StreamDecoder dec;
        auto packets = dec.feed(std::span<const std::uint8_t>(expected_bytes));
        CHECK_EQ(packets.size(), static_cast<std::size_t>(1));
        CHECK_EQ(dec.skippedByteCount(), static_cast<std::size_t>(0));
        if (!packets.empty()) {
            const std::string reason = ts::decoded_mismatch(packets[0], decoded);
            if (!reason.empty()) {
                CHECK(reason.empty());  // surfaces failure with file context below
                std::fprintf(stderr, "    [%s] %s\n", name.c_str(), reason.c_str());
            }
        }
        ++checked;
    }
    CHECK(checked >= 9);  // expect all nine single-packet vectors present
}

TEST_CASE("byte-split feed reassembles a single packet") {
    // Feeding one byte at a time must yield the same result (partial-receive, docs/05 L1).
    for (const auto& path : ts::vector_files()) {
        const json::Value v = json::parse(ts::read_file(path));
        if (!v.contains("packet_hex")) continue;

        const ts::Bytes bytes = ts::from_hex(v.at("packet_hex").as_string());
        proto::StreamDecoder dec;
        std::vector<proto::DecodedPacket> all;
        for (std::uint8_t b : bytes) {
            std::uint8_t one[1] = {b};
            auto got = dec.feed(std::span<const std::uint8_t>(one, 1));
            all.insert(all.end(), got.begin(), got.end());
        }
        CHECK_EQ(all.size(), static_cast<std::size_t>(1));
        CHECK_EQ(dec.skippedByteCount(), static_cast<std::size_t>(0));
        if (!all.empty()) {
            CHECK(ts::decoded_mismatch(all[0], v.at("decoded")).empty());
        }
    }
}
