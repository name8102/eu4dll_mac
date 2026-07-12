add_executable(eu4dll_text_rendering_tests text_rendering_test.cpp)
target_compile_features(eu4dll_text_rendering_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_text_rendering_tests PRIVATE
        eu4dll_portable_features)

add_test(NAME eu4dll.features.text_rendering COMMAND eu4dll_text_rendering_tests)
