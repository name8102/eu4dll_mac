# Preserve the existing constructor entry point and startup sequence verbatim.
add_library(eu4dll_bootstrap_macos OBJECT
        ${PROJECT_SOURCE_DIR}/library.cpp)
target_compile_features(eu4dll_bootstrap_macos PRIVATE cxx_std_17)
target_include_directories(eu4dll_bootstrap_macos PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_bootstrap_macos PRIVATE
        eu4dll_runtime
        eu4dll_target_profile_eu4_1_37_5_macos_x86_64)
