add_library(eu4dll_platform_macos_input STATIC
        input_method_adapter.cpp
        input_method_adapter.h)
target_compile_features(eu4dll_platform_macos_input PUBLIC cxx_std_17)
target_include_directories(eu4dll_platform_macos_input PUBLIC
        ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_platform_macos_input PUBLIC eu4dll_text_input)
