add_executable(eu4dll_linux_platform_tests linux_platform_tests.cpp)
target_compile_features(eu4dll_linux_platform_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_linux_platform_tests PRIVATE
        eu4dll_platform_linux
        eu4dll_runtime)
add_test(NAME eu4dll.platform.linux COMMAND eu4dll_linux_platform_tests)

add_executable(eu4dll_linux_input_adapter_tests sdl_input_adapter_tests.cpp)
target_link_libraries(eu4dll_linux_input_adapter_tests PRIVATE eu4dll_platform_linux_input)
add_test(NAME eu4dll.platform.linux_input COMMAND eu4dll_linux_input_adapter_tests)
