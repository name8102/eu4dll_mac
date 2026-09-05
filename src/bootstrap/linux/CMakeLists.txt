add_library(eu4dll_bootstrap_linux OBJECT
        bootstrap_linux.cpp
        bootstrap_linux.h
        entry.cpp)
target_compile_features(eu4dll_bootstrap_linux PRIVATE cxx_std_17)
target_include_directories(eu4dll_bootstrap_linux PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_bootstrap_linux PRIVATE
        eu4dll_runtime
        eu4dll_platform_linux
        eu4dll_target_profile_eu4_1_37_5_linux_x86_64
        eu4dll_target_base_eu4_1_37_5_linux_x86_64
        eu4dll_target_layout_eu4_1_37_5_linux_x86_64
        eu4dll_target_main_text_eu4_1_37_5_linux_x86_64
        eu4dll_target_tooltip_eu4_1_37_5_linux_x86_64
        eu4dll_target_localization_eu4_1_37_5_linux_x86_64
        eu4dll_target_map_text_eu4_1_37_5_linux_x86_64
        eu4dll_target_text_3d_eu4_1_37_5_linux_x86_64
        eu4dll_target_input_eu4_1_37_5_linux_x86_64
        eu4dll_target_display_eu4_1_37_5_linux_x86_64
        eu4dll_target_save_eu4_1_37_5_linux_x86_64
        eu4dll_target_search_eu4_1_37_5_linux_x86_64)
