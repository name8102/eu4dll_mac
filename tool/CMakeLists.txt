if(APPLE)
    project(insert_dylib)
    add_executable(insert_dylib insert_dylib.c)

    add_executable(eu4dll_manifest_tool manifest_tool.cpp)
    target_compile_features(eu4dll_manifest_tool PRIVATE cxx_std_17)
    target_include_directories(eu4dll_manifest_tool PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(eu4dll_manifest_tool PRIVATE
            eu4dll_macho_file
            eu4dll_runtime
            eu4dll_target_compatibility_eu4_1_37_macos_x86_64)
endif()
