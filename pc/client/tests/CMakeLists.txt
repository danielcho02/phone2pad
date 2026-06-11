# client_tests: Phase A unit/integration tests. Reuses the zero-dependency harness
# (test_main.hpp) and json_lite.hpp from proto/tests.
add_executable(client_tests
    test_entry.cpp
    mouse_sink_test.cpp
    frame_receiver_test.cpp
    trace_replay_test.cpp
)

target_link_libraries(client_tests PRIVATE client_core)
target_compile_features(client_tests PRIVATE cxx_std_20)
# test_main.hpp + json_lite.hpp live in the proto tests dir; mock_injector.hpp is local.
target_include_directories(client_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto/tests
)
target_compile_definitions(client_tests PRIVATE
    "PHONE2PAD_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../../tools/replay/fixtures\""
)
if(WIN32)
    target_compile_definitions(client_tests PRIVATE NOMINMAX)
endif()

add_test(NAME client_tests COMMAND client_tests)
