# phone2pad PC build root. Phase 0: protocol library + tests. Phase A adds the
# user-mode client (client/) and the replay/recorder tools (tools/). Phase C will
# add driver/ (not present yet).
cmake_minimum_required(VERSION 3.25)
project(phone2pad_pc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link the MSVC C runtime statically (/MT, /MTd for Debug) so the released exes run
# on a clean Windows install without the VC++ redistributable — the release zip
# stays self-contained (the client links only system libs: ws2_32, user32).
# CMP0091 (>=3.15, NEW here) makes MSVC_RUNTIME_LIBRARY the supported knob.
# https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html
if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(PHONE2PAD_BUILD_TESTS "Build unit tests" ON)

if(PHONE2PAD_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(client)
add_subdirectory(tools/replay)
add_subdirectory(tools/recorder)
