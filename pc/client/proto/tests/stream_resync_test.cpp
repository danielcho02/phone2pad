// Stream-resync tests for vectors with stream_hex + expected_packets + skipped_bytes
// (docs/02-PROTOCOL.md §7.2/§7.3). main() lives in packet_codec_test.cpp.
#include "test_main.hpp"

#include <span>
#include <string>
#include <vector>

#include "json_lite.hpp"
#include "phone2pad/proto/decoder.hpp"
#include "vector_support.hpp"

using namespace phone2pad;
namespace ts = phone2pad::testsupport;

namespace {

// Run a resync vector with a given feed strategy; checks recovered packets + skip count.
void run_resync(const json::Value& v, bool byte_split) {
    const std::string name = v.at("name").as_string();
    const ts::Bytes stream = ts::from_hex(v.at("stream_hex").as_string());
    const json::Array& expected = v.at("expected_packets").as_array();
    const auto expected_skipped = static_cast<std::size_t>(v.at("skipped_bytes").as_int());

    proto::StreamDecoder dec;
    std::vector<proto::DecodedPacket> got;
    if (byte_split) {
        for (std::uint8_t b : stream) {
            std::uint8_t one[1] = {b};
            auto p = dec.feed(std::span<const std::uint8_t>(one, 1));
            got.insert(got.end(), p.begin(), p.end());
        }
    } else {
        got = dec.feed(std::span<const std::uint8_t>(stream));
    }

    CHECK_EQ(got.size(), expected.size());
    CHECK_EQ(dec.skippedByteCount(), expected_skipped);

    const std::size_t n = std::min(got.size(), expected.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::string reason = ts::decoded_mismatch(got[i], expected[i]);
        if (!reason.empty()) {
            CHECK(reason.empty());
            std::fprintf(stderr, "    [%s] packet %zu: %s\n", name.c_str(), i, reason.c_str());
        }
    }
}

}  // namespace

TEST_CASE("resync vectors recover all packets and count skipped bytes (whole feed)") {
    int checked = 0;
    for (const auto& path : ts::vector_files()) {
        const json::Value v = json::parse(ts::read_file(path));
        if (!v.contains("stream_hex")) continue;
        run_resync(v, /*byte_split=*/false);
        ++checked;
    }
    CHECK(checked >= 1);
}

TEST_CASE("resync vectors are identical under byte-split feed") {
    for (const auto& path : ts::vector_files()) {
        const json::Value v = json::parse(ts::read_file(path));
        if (!v.contains("stream_hex")) continue;
        run_resync(v, /*byte_split=*/true);
    }
}
