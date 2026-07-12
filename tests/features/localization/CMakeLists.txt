add_executable(eu4dll_localization_feature_tests
        localization_feature_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/targets/eu4_1_37_5/macos_x86_64/localization_features/localization_patch_contract.cpp)
target_compile_features(eu4dll_localization_feature_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_localization_feature_tests PRIVATE
        eu4dll_localization_features
        eu4dll_runtime)
add_test(NAME eu4dll.features.localization COMMAND eu4dll_localization_feature_tests)

add_executable(eu4dll_pinyin_integration_tests
        pinyin_integration_test.cpp
        ${PROJECT_SOURCE_DIR}/src/pinyinHelper.cpp)
target_compile_features(eu4dll_pinyin_integration_tests PRIVATE cxx_std_17)
target_include_directories(eu4dll_pinyin_integration_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_pinyin_integration_tests PRIVATE
        eu4dll_localization_features
        cpp-pinyin)
target_compile_definitions(eu4dll_pinyin_integration_tests PRIVATE
        EU4DLL_PINYIN_DICTIONARY="${cpp-pinyin_SOURCE_DIR}/res/dict")
add_test(NAME eu4dll.features.pinyin_integration COMMAND eu4dll_pinyin_integration_tests)
