# recorder: capture phone touch frames to .pptrace. Reuses client_core.
add_executable(recorder main.cpp)
target_link_libraries(recorder PRIVATE client_core)
target_compile_features(recorder PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(recorder PRIVATE NOMINMAX)
endif()
