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
    src/process_runner.cpp
    src/net_client.cpp
    src/client_service.cpp
    src/autostart.cpp
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

# phone2pad_tray: the v0.3.0 background/tray companion (Windows-only). GUI subsystem
# (WIN32) so it runs with no console window. Drives the same client_core pipeline and
# adds Shell_NotifyIcon UI + HKCU autostart (shell32/advapi32 are system libs, so the
# release zip stays self-contained).
if(WIN32)
    # tray.rc embeds assets/phone2pad.ico as the app/tray icon (VS generator compiles
    # the .rc automatically; rc.exe resolves the icon path relative to the .rc file).
    add_executable(phone2pad_tray WIN32 src/tray_main.cpp tray.rc)
    target_link_libraries(phone2pad_tray PRIVATE client_core shell32 advapi32)
    # tray_resource.h lives in this dir (shared by tray_main.cpp and tray.rc).
    target_include_directories(phone2pad_tray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(phone2pad_tray PRIVATE cxx_std_20)
    target_compile_definitions(phone2pad_tray PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    if(MSVC)
        # The sources are UTF-8; tell MSVC so the tray's Korean wide-string literals
        # decode correctly (otherwise cp949 is assumed — see the C4819 warnings).
        target_compile_options(phone2pad_tray PRIVATE /utf-8)
    endif()
endif()

if(PHONE2PAD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
