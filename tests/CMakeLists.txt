add_executable(eu4dll_build_skeleton_tests build_skeleton.cpp)
target_compile_features(eu4dll_build_skeleton_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_build_skeleton_tests PRIVATE
        eu4dll_portable_features
        eu4dll_runtime)

function(assert_target_exists target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Required build-skeleton target is missing: ${target_name}")
    endif()
endfunction()

function(assert_list_contains list_name expected_value description)
    list(FIND ${list_name} "${expected_value}" value_index)
    if(value_index EQUAL -1)
        message(FATAL_ERROR "Build-skeleton check failed: ${description}")
    endif()
endfunction()

if(APPLE)
    foreach(required_target IN ITEMS
            eu4dll_portable_features
            eu4dll_text_input
            eu4dll_localization_features
            eu4dll_runtime
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64
            eu4dll_target_profile_eu4_1_37_5_macos_x86_64
            eu4dll_platform_macos
            eu4dll_macho_file
            eu4dll_platform_macos_input
            eu4dll_target_input_eu4_1_37_5_macos_x86_64
            eu4dll_target_eu4_1_37_5_macos_x86_64
            eu4dll_bootstrap_macos
            eu4dll_mac)
        assert_target_exists(${required_target})
    endforeach()

    get_target_property(EU4DLL_MAC_SOURCES eu4dll_mac SOURCES)
    assert_list_contains(EU4DLL_MAC_SOURCES
            "$<TARGET_OBJECTS:eu4dll_bootstrap_macos>"
            "eu4dll_mac must include the macOS bootstrap objects")
    assert_list_contains(EU4DLL_MAC_SOURCES
            "$<TARGET_OBJECTS:eu4dll_platform_macos>"
            "eu4dll_mac must include the macOS platform objects")
    assert_list_contains(EU4DLL_MAC_SOURCES
            "$<TARGET_OBJECTS:eu4dll_target_eu4_1_37_5_macos_x86_64>"
            "eu4dll_mac must include the selected EU4 target objects")

    get_target_property(EU4DLL_MAC_LINK_LIBRARIES eu4dll_mac LINK_LIBRARIES)
    foreach(required_link IN ITEMS
            eu4dll_platform_macos_input
            eu4dll_target_input_eu4_1_37_5_macos_x86_64
            eu4dll_localization_features
            eu4dll_runtime
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64
            eu4dll_target_profile_eu4_1_37_5_macos_x86_64
            cpp-pinyin
            "${CORE_FOUNDATION_LIB}")
        assert_list_contains(EU4DLL_MAC_LINK_LIBRARIES "${required_link}"
                "eu4dll_mac is missing required link dependency ${required_link}")
    endforeach()

    add_test(NAME eu4dll.build_skeleton COMMAND eu4dll_build_skeleton_tests)
    add_test(NAME eu4dll.build_graph
            COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
            --target eu4dll_mac eu4dll_build_skeleton_tests)
    add_test(NAME eu4dll.installer.fake_app
            COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/install_fake_app.sh")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(required_target IN ITEMS
            eu4dll_portable_features
            eu4dll_runtime
            eu4dll_platform_linux
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_target_base_eu4_1_37_5_linux_x86_64
            eu4dll_target_layout_eu4_1_37_5_linux_x86_64
            eu4dll_target_main_text_eu4_1_37_5_linux_x86_64
            eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64
            eu4dll_bootstrap_linux
            eu4dll_linux)
        assert_target_exists(${required_target})
    endforeach()

    get_target_property(EU4DLL_LINUX_SOURCES eu4dll_linux SOURCES)
    assert_list_contains(EU4DLL_LINUX_SOURCES
            "$<TARGET_OBJECTS:eu4dll_bootstrap_linux>"
            "eu4dll_linux must include the Linux bootstrap objects")

    add_test(NAME eu4dll.build_skeleton COMMAND eu4dll_build_skeleton_tests)
    add_test(NAME eu4dll.build_graph
            COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
            --target eu4dll_linux eu4dll_build_skeleton_tests)
endif()

add_subdirectory(features)
add_subdirectory(runtime)
add_subdirectory(targets)
if(APPLE)
    add_subdirectory(platform/macos/input)

    add_executable(eu4dll_macho_file_tests platform/macos/macho_file_tests.cpp)
    target_compile_features(eu4dll_macho_file_tests PRIVATE cxx_std_17)
    target_link_libraries(eu4dll_macho_file_tests PRIVATE eu4dll_macho_file)
    add_test(NAME eu4dll.platform.macho_file COMMAND eu4dll_macho_file_tests)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(platform/linux)
endif()

if(APPLE)
    add_custom_target(eu4dll_tests DEPENDS
            eu4dll_build_skeleton_tests
            eu4dll_escaped_text_tests
            eu4dll_text_input_tests
            eu4dll_macos_input_adapter_tests
            eu4dll_text_rendering_tests
            eu4dll_text_rendering_target_contract_tests
            eu4dll_localization_feature_tests
            eu4dll_pinyin_integration_tests
            eu4dll_patch_runtime_tests
            eu4dll_branch_resolver_tests
            eu4dll_patch_batch_tests
            eu4dll_multi_region_tests
            eu4dll_startup_diagnostics_tests
            eu4dll_patch_manifest_tests
            eu4dll_macho_file_tests
            eu4dll_macos_target_profile_tests
            eu4dll_compatibility_preflight_tests
            eu4dll_real_binary_probe
            eu4dll_input_bridge_tests
            eu4dll_mac)
else()
    add_custom_target(eu4dll_tests DEPENDS
            eu4dll_build_skeleton_tests
            eu4dll_escaped_text_tests
            eu4dll_text_input_tests
            eu4dll_text_rendering_tests
            eu4dll_localization_feature_tests
            eu4dll_pinyin_integration_tests
            eu4dll_patch_runtime_tests
            eu4dll_branch_resolver_tests
            eu4dll_patch_batch_tests
            eu4dll_multi_region_tests
            eu4dll_startup_diagnostics_tests
            eu4dll_patch_manifest_tests
            eu4dll_linux_target_tests
            eu4dll_linux_text_layout_tests
            eu4dll_linux_main_text_tests
            eu4dll_linux_tooltip_tests
            eu4dll_linux_platform_tests
            eu4dll_linux)
endif()
