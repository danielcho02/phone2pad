# replay: trace playback tool. Reuses client_core (trace reader, replay, MouseSink).
add_executable(replay main.cpp)
target_link_libraries(replay PRIVATE client_core)
target_compile_features(replay PRIVATE cxx_std_20)
if(WIN32)
    target_compile_definitions(replay PRIVATE NOMINMAX)
endif()
