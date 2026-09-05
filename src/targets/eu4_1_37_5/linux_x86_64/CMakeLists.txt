add_library(eu4dll_target_profile_eu4_1_37_5_linux_x86_64 STATIC
        profile.cpp
        profile.h
        target_facts.h)
target_compile_features(eu4dll_target_profile_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_profile_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_target_profile_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_platform_linux)

add_library(eu4dll_target_base_eu4_1_37_5_linux_x86_64 STATIC
        base/base_patch.cpp
        base/base_patch.h)
target_compile_features(eu4dll_target_base_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_base_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_target_base_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64)
