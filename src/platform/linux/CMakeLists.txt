add_library(eu4dll_platform_linux STATIC
        linux_process_memory.cpp
        linux_process_memory.h
        linux_executable_allocator.cpp
        linux_executable_allocator.h
        linux_elf_identity.cpp
        linux_elf_identity.h)
target_compile_features(eu4dll_platform_linux PUBLIC cxx_std_17)
target_include_directories(eu4dll_platform_linux PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_platform_linux PUBLIC eu4dll_runtime dl)
