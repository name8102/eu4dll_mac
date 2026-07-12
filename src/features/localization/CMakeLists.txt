add_library(eu4dll_localization_features STATIC
        ../save_filenames/save_filenames.cpp
        ../save_filenames/save_filenames.h
        ../localized_search/localized_search.cpp
        ../localized_search/localized_search.h
        ../east_asian_names/east_asian_names.cpp
        ../east_asian_names/east_asian_names.h
        ../localization_loading/localization_loading.cpp
        ../localization_loading/localization_loading.h
        ../date_formatting/date_formatting.h)
target_compile_features(eu4dll_localization_features PUBLIC cxx_std_17)
target_include_directories(eu4dll_localization_features PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(eu4dll_localization_features PUBLIC eu4dll_portable_features)
