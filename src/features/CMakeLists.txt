add_library(eu4dll_portable_features STATIC
        module.cpp
        escaped_text/escaped_text.cpp
        escaped_text/escaped_text.h)
target_compile_features(eu4dll_portable_features PUBLIC cxx_std_17)
target_include_directories(eu4dll_portable_features PUBLIC
        ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(text_input)
add_subdirectory(text_rendering)
add_subdirectory(localization)
