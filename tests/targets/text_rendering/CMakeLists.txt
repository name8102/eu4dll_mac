add_executable(eu4dll_text_rendering_target_contract_tests
        rendering_contract_test.cpp)
target_compile_features(eu4dll_text_rendering_target_contract_tests PRIVATE cxx_std_17)
target_link_libraries(eu4dll_text_rendering_target_contract_tests PRIVATE
        eu4dll_text_rendering_target_contract)

add_test(NAME eu4dll.target.text_rendering_contract
        COMMAND eu4dll_text_rendering_target_contract_tests)
