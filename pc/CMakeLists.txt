# phone2pad PC build root. Phase 0: protocol library + tests. Phase A adds the
# user-mode client (client/) and the replay/recorder tools (tools/). Phase C will
# add driver/ (not present yet).
cmake_minimum_required(VERSION 3.25)
project(phantompad_pc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PHANTOMPAD_BUILD_TESTS "Build unit tests" ON)

if(PHANTOMPAD_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(client)
add_subdirectory(tools/replay)
add_subdirectory(tools/recorder)
