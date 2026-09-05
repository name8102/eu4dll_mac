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

add_library(eu4dll_target_layout_eu4_1_37_5_linux_x86_64 STATIC
        text_layout/layout_patch.cpp
        text_layout/layout_patch.h)
target_compile_features(eu4dll_target_layout_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_layout_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
# The naked decoders reuse portable escaped_text marker/shift policy;
# Linux glyph-table/index facts stay in the target.
target_link_libraries(eu4dll_target_layout_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_portable_features
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64)

add_library(eu4dll_target_main_text_eu4_1_37_5_linux_x86_64 STATIC
        main_text/main_text_patch.cpp
        main_text/main_text_patch.h)
target_compile_features(eu4dll_target_main_text_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_main_text_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
# Same policy split as layout: portable escaped_text constants in hooks,
# Linux ABI/layout facts in the target.
target_link_libraries(eu4dll_target_main_text_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_portable_features
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64)

add_library(eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64 STATIC
        tooltip_text/tooltip_patch.cpp
        tooltip_text/tooltip_patch.h)
target_compile_features(eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
# Same policy split as layout/main-text; the CString callee resolves through
# Memory::ResolveSymbol at install, never hard-coded.
target_link_libraries(eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_portable_features
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64)

add_library(eu4dll_target_localization_eu4_1_37_5_linux_x86_64 STATIC
        localization_text/localization_patch.cpp
        localization_text/localization_patch.h)
target_compile_features(eu4dll_target_localization_eu4_1_37_5_linux_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_localization_eu4_1_37_5_linux_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
# The bridge delegates to the portable localization_loading feature.
target_link_libraries(eu4dll_target_localization_eu4_1_37_5_linux_x86_64 PUBLIC
        eu4dll_runtime
        eu4dll_localization_features
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64)
