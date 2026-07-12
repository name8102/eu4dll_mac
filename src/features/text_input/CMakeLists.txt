add_library(eu4dll_text_input STATIC
        text_input.cpp
        text_input.h)
target_compile_features(eu4dll_text_input PUBLIC cxx_std_17)
target_include_directories(eu4dll_text_input PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_text_input PUBLIC eu4dll_portable_features)
