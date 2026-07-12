add_library(eu4dll_runtime STATIC
        diagnostics/patch_diagnostic.cpp
        diagnostics/patch_diagnostic.h
        diagnostics/startup_diagnostics.cpp
        diagnostics/startup_diagnostics.h
        manifest/patch_manifest.cpp
        manifest/patch_manifest.h
        patch/byte_buffer_memory.cpp
        patch/byte_buffer_memory.h
        patch/memory.h
        patch/patch_runtime.cpp
        patch/patch_runtime.h)
target_compile_features(eu4dll_runtime PUBLIC cxx_std_17)
target_include_directories(eu4dll_runtime PUBLIC ${PROJECT_SOURCE_DIR}/src)
