cmake_minimum_required(VERSION 3.27)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build for x86_64 only" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Avoid inheriting the build host SDK minimum; verified against EU4 1.37.x" FORCE)
endif()

project(eu4dll_mac)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
set(CPP_PINYIN_BUILD_STATIC ON CACHE BOOL "Build cpp-pinyin as a static library" FORCE)
set(CPP_PINYIN_INSTALL OFF CACHE BOOL "Do not install cpp-pinyin from this project" FORCE)
FetchContent_Declare(
        cpp-pinyin
        GIT_REPOSITORY https://github.com/wolfgitpr/cpp-pinyin.git
        GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(cpp-pinyin)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Keep project warnings fatal without making pinned third-party warnings
    # release-blocking. The warnings remain visible in strict builds.
    target_compile_options(cpp-pinyin PRIVATE -Wno-error)
endif()

# Portable modules always configure on every supported host.
add_subdirectory(src/features)
add_subdirectory(src/runtime)

if(APPLE)
    find_library(CORE_FOUNDATION_LIB CoreFoundation)

    add_subdirectory(src/platform/macos)
    add_subdirectory(src/targets/eu4_1_37_5/macos_x86_64)
    add_subdirectory(src/bootstrap/macos)

    add_library(eu4dll_mac SHARED
            $<TARGET_OBJECTS:eu4dll_bootstrap_macos>
            $<TARGET_OBJECTS:eu4dll_platform_macos>
            $<TARGET_OBJECTS:eu4dll_target_eu4_1_37_5_macos_x86_64>)
    target_link_libraries(eu4dll_mac PRIVATE
            eu4dll_platform_macos_input
            eu4dll_target_input_eu4_1_37_5_macos_x86_64
            eu4dll_localization_features
            eu4dll_runtime
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64
            eu4dll_target_profile_eu4_1_37_5_macos_x86_64
            cpp-pinyin
            ${CORE_FOUNDATION_LIB})

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${cpp-pinyin_SOURCE_DIR}/res/dict"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/chinese_dict"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(src/platform/linux)
    add_subdirectory(src/targets/eu4_1_37_5/linux_x86_64)
    add_subdirectory(src/bootstrap/linux)

    # Preloadable Linux shared library (LD_PRELOAD). Only the base vertical
    # slice is enabled in the foundation task.
    add_library(eu4dll_linux SHARED
            $<TARGET_OBJECTS:eu4dll_bootstrap_linux>)
    target_link_libraries(eu4dll_linux PRIVATE
            eu4dll_runtime
            eu4dll_platform_linux
            eu4dll_target_profile_eu4_1_37_5_linux_x86_64
            eu4dll_target_base_eu4_1_37_5_linux_x86_64
            cpp-pinyin
            dl)

    add_custom_command(TARGET eu4dll_linux POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${cpp-pinyin_SOURCE_DIR}/res/dict"
            "$<TARGET_FILE_DIR:eu4dll_linux>/chinese_dict"
    )
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(tool)
if(BUILD_TESTING AND APPLE)
    add_dependencies(eu4dll_tests insert_dylib)
endif()
