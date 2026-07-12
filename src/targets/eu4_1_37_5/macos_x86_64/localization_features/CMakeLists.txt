# Included by the selected EU4 target after the legacy localization sources are
# removed from its parent source list.
target_sources(eu4dll_target_eu4_1_37_5_macos_x86_64 PRIVATE
        localization_patch.cpp
        localization_patch_contract.cpp
        localization_patch.h
        ${PROJECT_SOURCE_DIR}/src/saveFileName.cpp
        ${PROJECT_SOURCE_DIR}/src/saveFileName.h
        ${PROJECT_SOURCE_DIR}/src/localization.cpp
        ${PROJECT_SOURCE_DIR}/src/localization.h
        ${PROJECT_SOURCE_DIR}/src/pinyinHelper.cpp
        ${PROJECT_SOURCE_DIR}/src/pinyinHelper.h)
target_link_libraries(eu4dll_target_eu4_1_37_5_macos_x86_64 PRIVATE
        eu4dll_localization_features
        cpp-pinyin)
