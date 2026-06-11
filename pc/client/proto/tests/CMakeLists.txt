# proto_tests: round-trip + stream-resync tests against the shared vectors.
# No external test framework (offline-friendly); see test_main.hpp / json_lite.hpp.
add_executable(proto_tests
    packet_codec_test.cpp
    stream_resync_test.cpp
)

target_link_libraries(proto_tests PRIVATE proto)
target_compile_features(proto_tests PRIVATE cxx_std_20)

# Absolute path to the shared test-vector directory (../../../../proto/test-vectors
# relative to this file: pc/client/proto/tests -> repo root -> proto/test-vectors).
get_filename_component(_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
target_compile_definitions(proto_tests PRIVATE
    "PHONE2PAD_TEST_VECTOR_DIR=\"${_repo_root}/proto/test-vectors\""
)

add_test(NAME proto_tests COMMAND proto_tests)
