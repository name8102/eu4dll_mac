add_executable(eu4dll_macos_input_adapter_tests input_method_adapter_test.cpp)
target_compile_features(eu4dll_macos_input_adapter_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_macos_input_adapter_tests PRIVATE
        eu4dll_platform_macos_input)
add_test(NAME eu4dll.macos_input_adapter
        COMMAND eu4dll_macos_input_adapter_tests)
