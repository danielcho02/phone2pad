# PC client subtree. proto (Phase 0 codec) + client_core (Phase A receive/map/inject)
# + the phone2pad_client executable.
add_subdirectory(proto)

# client_core: reused by the exe, the replay/recorder tools, and unit tests.
add_library(client_core STATIC
    src/frame_receiver.cpp
    src/mouse_sink.cpp
    src/gesture_sink.cpp
    src/gesture_router.cpp
    src/orientation_normalizer.cpp
    src/adb_manager.cpp
    src/net_client.cpp
    src/trace.cpp
    src/replay.cpp
    src/win32_input_injector.cpp
)

target_include_directories(client_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# json_lite.hpp is a header-only JSON reader vendored under proto/tests; trace.cpp
# reuses it (PRIVATE — not part of client_core's public surface).
target_include_directories(client_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/proto/tests)
target_compile_features(client_core PUBLIC cxx_std_20)
target_link_libraries(client_core PUBLIC proto)

if(WIN32)
    target_link_libraries(client_core PUBLIC ws2_32 user32)
    target_compile_definitions(client_core PRIVATE
        _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_executable(phone2pad_client src/main.cpp)
target_link_libraries(phone2pad_client PRIVATE client_core)
target_compile_features(phone2pad_client PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(phone2pad_client PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(PHONE2PAD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
