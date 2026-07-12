add_library(eu4dll_target_profile_eu4_1_37_5_macos_x86_64 STATIC
        hook_symbols.cpp
        hook_symbols.h
        profile.cpp
        profile.h
        target_facts.h)
target_compile_features(eu4dll_target_profile_eu4_1_37_5_macos_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_profile_eu4_1_37_5_macos_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
add_subdirectory(input)
# These legacy hook adapters still combine some feature policy with target ABI.
# Their executable literals live in target_facts.h while later vertical slices
# continue moving portable policy out of these adapters.
add_library(eu4dll_target_eu4_1_37_5_macos_x86_64 OBJECT
        ${PROJECT_SOURCE_DIR}/src/base.cpp
        ${PROJECT_SOURCE_DIR}/src/base.h
        ${PROJECT_SOURCE_DIR}/src/mainText.cpp
        ${PROJECT_SOURCE_DIR}/src/mainText.h
        ${PROJECT_SOURCE_DIR}/src/tooltipAndButtonText.cpp
        ${PROJECT_SOURCE_DIR}/src/tooltipAndButtonText.h
        ${PROJECT_SOURCE_DIR}/src/mapText.cpp
        ${PROJECT_SOURCE_DIR}/src/mapText.h
        ${PROJECT_SOURCE_DIR}/src/text3D.cpp
        ${PROJECT_SOURCE_DIR}/src/text3D.h
        ${PROJECT_SOURCE_DIR}/src/textLayout.cpp
        ${PROJECT_SOURCE_DIR}/src/textLayout.h
        ${PROJECT_SOURCE_DIR}/src/input.cpp
        ${PROJECT_SOURCE_DIR}/src/input.h)
target_compile_features(eu4dll_target_eu4_1_37_5_macos_x86_64 PRIVATE cxx_std_17)
target_link_libraries(eu4dll_target_eu4_1_37_5_macos_x86_64 PRIVATE
        eu4dll_portable_features
        eu4dll_text_input
        eu4dll_platform_macos_input
        eu4dll_target_input_eu4_1_37_5_macos_x86_64
        eu4dll_runtime
        eu4dll_target_profile_eu4_1_37_5_macos_x86_64
        cpp-pinyin)

add_subdirectory(text_rendering)
add_subdirectory(localization_features)

add_library(eu4dll_target_compatibility_eu4_1_37_macos_x86_64 STATIC
        compatibility_preflight.cpp
        compatibility_preflight.h)
target_compile_features(eu4dll_target_compatibility_eu4_1_37_macos_x86_64
        PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_compatibility_eu4_1_37_macos_x86_64
        PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_target_compatibility_eu4_1_37_macos_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_target_profile_eu4_1_37_5_macos_x86_64
        eu4dll_text_rendering_target_contract)
