add_library(eu4dll_text_rendering_target_contract STATIC
        ABI_NOTES.md
        VERIFICATION.md
        rendering_contract.cpp
        rendering_contract.h)
target_compile_features(eu4dll_text_rendering_target_contract PUBLIC cxx_std_17)
target_include_directories(eu4dll_text_rendering_target_contract PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_text_rendering_target_contract PUBLIC
        eu4dll_runtime
        eu4dll_target_profile_eu4_1_37_5_macos_x86_64)

target_sources(eu4dll_target_eu4_1_37_5_macos_x86_64 PRIVATE
        rendering_contract.cpp
        rendering_contract.h
        rendering_patch.cpp
        rendering_patch.h)
