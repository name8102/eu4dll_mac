add_library(eu4dll_platform_macos OBJECT
        live_patch_runtime.cpp
        live_patch_runtime.h
        mach_process_memory.cpp
        mach_process_memory.h
        symbol_lookup.cpp
        symbol_lookup.h)
target_compile_features(eu4dll_platform_macos PRIVATE cxx_std_17)
target_include_directories(eu4dll_platform_macos PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_platform_macos PRIVATE eu4dll_runtime)

add_library(eu4dll_macho_file STATIC
        macho_file.cpp
        macho_file.h)
target_compile_features(eu4dll_macho_file PUBLIC cxx_std_17)
target_include_directories(eu4dll_macho_file PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_macho_file PUBLIC eu4dll_runtime)

add_subdirectory(input)
