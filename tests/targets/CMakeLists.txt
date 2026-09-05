if(APPLE)
    add_executable(eu4dll_macos_target_profile_tests macos_target_profile_tests.cpp)
    target_compile_features(eu4dll_macos_target_profile_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_macos_target_profile_tests PRIVATE
            eu4dll_target_profile_eu4_1_37_5_macos_x86_64
            eu4dll_runtime)

    add_test(NAME eu4dll.target.macos_profile COMMAND eu4dll_macos_target_profile_tests)

    add_executable(eu4dll_compatibility_preflight_tests compatibility_preflight_test.cpp)
    target_compile_features(eu4dll_compatibility_preflight_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_compatibility_preflight_tests PRIVATE
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.compatibility_preflight
            COMMAND eu4dll_compatibility_preflight_tests)

    add_executable(eu4dll_real_binary_probe real_binary_probe.cpp)
    target_compile_features(eu4dll_real_binary_probe PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_real_binary_probe PRIVATE
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64
            eu4dll_target_profile_eu4_1_37_5_macos_x86_64)

    add_executable(eu4dll_input_bridge_tests input_bridge_test.cpp)
    target_compile_features(eu4dll_input_bridge_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_input_bridge_tests PRIVATE
            eu4dll_target_input_eu4_1_37_5_macos_x86_64)
    add_test(NAME eu4dll.target.input_bridge COMMAND eu4dll_input_bridge_tests)

    add_subdirectory(text_rendering)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(eu4dll_linux_target_tests linux_target_tests.cpp)
    target_compile_features(eu4dll_linux_target_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_target_tests PRIVATE
            eu4dll_target_base_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_profile COMMAND eu4dll_linux_target_tests)

    add_executable(eu4dll_linux_text_layout_tests linux_text_layout_tests.cpp)
    target_compile_features(eu4dll_linux_text_layout_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_text_layout_tests PRIVATE
            eu4dll_target_layout_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_text_layout
            COMMAND eu4dll_linux_text_layout_tests)

    add_executable(eu4dll_linux_main_text_tests linux_main_text_tests.cpp)
    target_compile_features(eu4dll_linux_main_text_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_main_text_tests PRIVATE
            eu4dll_target_main_text_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_main_text
            COMMAND eu4dll_linux_main_text_tests)

    add_executable(eu4dll_linux_tooltip_tests linux_tooltip_tests.cpp)
    target_compile_features(eu4dll_linux_tooltip_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_tooltip_tests PRIVATE
            eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_tooltip
            COMMAND eu4dll_linux_tooltip_tests)

    add_executable(eu4dll_linux_localization_tests linux_localization_tests.cpp)
    target_compile_features(eu4dll_linux_localization_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_localization_tests PRIVATE
            eu4dll_target_localization_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_localization
            COMMAND eu4dll_linux_localization_tests)

    add_executable(eu4dll_linux_map_text_tests linux_map_text_tests.cpp)
    target_compile_features(eu4dll_linux_map_text_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_linux_map_text_tests PRIVATE
            eu4dll_target_map_text_eu4_1_37_5_linux_x86_64
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_runtime)
    add_test(NAME eu4dll.target.linux_map_text
            COMMAND eu4dll_linux_map_text_tests)
endif()
