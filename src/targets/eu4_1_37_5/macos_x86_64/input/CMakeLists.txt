add_library(eu4dll_target_input_eu4_1_37_5_macos_x86_64 STATIC
        input_bridge.cpp
        input_bridge.h)
target_compile_features(eu4dll_target_input_eu4_1_37_5_macos_x86_64 PUBLIC cxx_std_17)
target_include_directories(eu4dll_target_input_eu4_1_37_5_macos_x86_64 PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
