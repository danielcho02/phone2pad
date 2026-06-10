# proto: zero-dependency wire-protocol codec (SSOT: docs/02-PROTOCOL.md).
add_library(proto STATIC
    src/encoder.cpp
    src/decoder.cpp
)

target_include_directories(proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(proto PUBLIC cxx_std_20)

if(PHANTOMPAD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
