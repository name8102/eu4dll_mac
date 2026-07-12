cmake_minimum_required(VERSION 3.27)
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build for x86_64 only" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
    "Avoid inheriting the build host SDK minimum; verified against EU4 1.37.x" FORCE)

project(eu4dll_mac)

if(NOT APPLE)
    message(FATAL_ERROR "eu4dll_mac currently supports only the macOS build target")
endif()

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

find_library(CORE_FOUNDATION_LIB CoreFoundation)

# The migration starts with build-graph boundaries. Existing source remains in
# its current files until a later child task can move one behavior at a time.
add_subdirectory(src/features)
add_subdirectory(src/runtime)
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

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(tool)
if(BUILD_TESTING)
    add_dependencies(eu4dll_tests insert_dylib)
endif()
