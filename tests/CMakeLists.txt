add_executable(stroke_contract_tests unit/contracts.cpp)
target_link_libraries(stroke_contract_tests PRIVATE stroke::tsf_contract)
stroke_configure_target(stroke_contract_tests)
add_test(NAME framework.contracts COMMAND stroke_contract_tests)
add_executable(stroke_engine_tests unit/engine.cpp)
target_link_libraries(stroke_engine_tests PRIVATE stroke::engine)
stroke_configure_target(stroke_engine_tests)
add_test(NAME engine.behavior COMMAND stroke_engine_tests)
add_executable(stroke_learning_tests unit/learning.cpp)
target_link_libraries(stroke_learning_tests PRIVATE stroke::engine)
stroke_configure_target(stroke_learning_tests)
add_test(NAME engine.learning COMMAND stroke_learning_tests)
if(WIN32 AND MSVC)
    add_executable(stroke_tsf_host_tests unit/tsf_host.cpp "${PROJECT_SOURCE_DIR}/src/windows/ui/candidate_window.cpp")
    target_link_libraries(stroke_tsf_host_tests PRIVATE stroke::engine stroke_layout_store ole32 oleaut32 uuid user32 gdi32 advapi32)
    target_compile_definitions(stroke_tsf_host_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    stroke_configure_target(stroke_tsf_host_tests)
    add_dependencies(stroke_tsf_host_tests stroke_tsf)
    add_test(NAME windows.tsf_host COMMAND stroke_tsf_host_tests "$<TARGET_FILE:stroke_tsf>")
    add_executable(stroke_learning_store_tests unit/learning_store.cpp)
    target_link_libraries(stroke_learning_store_tests PRIVATE stroke_layout_store)
    target_compile_definitions(stroke_learning_store_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    stroke_configure_target(stroke_learning_store_tests)
    add_test(NAME windows.learning_store COMMAND stroke_learning_store_tests)
endif()
add_executable(stroke_frequency_tests unit/frequency.cpp)
target_link_libraries(stroke_frequency_tests PRIVATE stroke::engine)
stroke_configure_target(stroke_frequency_tests)
add_test(NAME dictionary.frequency COMMAND stroke_frequency_tests)

add_executable(stroke_wildcard_tests unit/wildcard.cpp)
target_link_libraries(stroke_wildcard_tests PRIVATE stroke::engine)
stroke_configure_target(stroke_wildcard_tests)
add_test(NAME engine.wildcard COMMAND stroke_wildcard_tests)

add_executable(stroke_keyboard_tests unit/keyboard.cpp)
stroke_configure_target(stroke_keyboard_tests)
add_test(NAME windows.keyboard_policy COMMAND stroke_keyboard_tests)

add_executable(stroke_layout_tests unit/layout.cpp)
target_link_libraries(stroke_layout_tests PRIVATE stroke::engine)
if(WIN32 AND MSVC)
    target_link_libraries(stroke_layout_tests PRIVATE stroke_layout_store)
    target_compile_definitions(stroke_layout_tests PRIVATE STROKE_HAS_LAYOUT_STORE)
endif()
stroke_configure_target(stroke_layout_tests)
add_test(NAME settings.layouts COMMAND stroke_layout_tests)

# Compile each public header on its own to prevent accidental transitive-include dependencies.
set(public_headers
    stroke/domain/types.hpp stroke/domain/commands.hpp stroke/domain/config.hpp
    stroke/dictionary/dictionary.hpp stroke/dictionary/index.hpp stroke/dictionary/text.hpp
    stroke/engine/session.hpp
    stroke/storage/config_store.hpp stroke/ui/candidate_view.hpp stroke/tsf/context_token.hpp)
set(header_sources)
foreach(header IN LISTS public_headers)
    string(MAKE_C_IDENTIFIER "${header}" header_id)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${header_id}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
endforeach()
add_library(stroke_header_checks OBJECT ${header_sources})
target_link_libraries(stroke_header_checks PRIVATE stroke::tsf_contract)
stroke_configure_target(stroke_header_checks)

if(STROKE_BUILD_TOOLS)
    add_test(NAME dictionary.frequency_source COMMAND ${CMAKE_COMMAND}
        "-DBUILDER=$<TARGET_FILE:stroke_dict_builder>"
        "-DTESTER=$<TARGET_FILE:stroke_frequency_tests>"
        "-DSOURCE=${PROJECT_SOURCE_DIR}/data/upstream/libchewing-data"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/frequency-data"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/integration/frequency.cmake")
    add_test(NAME engine.real_data COMMAND ${CMAKE_COMMAND}
        "-DBUILDER=$<TARGET_FILE:stroke_dict_builder>"
        "-DCONSOLE=$<TARGET_FILE:stroke_engine_console>"
        "-DSOURCE=${PROJECT_SOURCE_DIR}/data/upstream/conway"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/engine-real-data"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/integration/engine.cmake")
    add_test(NAME tool.version COMMAND stroke_dict_builder --version)
    add_test(NAME tool.help COMMAND stroke_dict_builder --help)
    add_test(NAME tool.reject_missing_arguments COMMAND stroke_dict_builder build)
    set_tests_properties(tool.reject_missing_arguments PROPERTIES WILL_FAIL TRUE)
    add_executable(stroke_dictionary_tests unit/dictionary.cpp)
    target_link_libraries(stroke_dictionary_tests PRIVATE stroke::dictionary_build)
    stroke_configure_target(stroke_dictionary_tests)
    add_test(NAME dictionary.contracts COMMAND stroke_dictionary_tests)
    add_test(NAME dictionary.full_conway COMMAND stroke_dictionary_tests
        "${PROJECT_SOURCE_DIR}/data/upstream/conway")
    add_test(NAME dictionary.cli COMMAND ${CMAKE_COMMAND}
        "-DTOOL=$<TARGET_FILE:stroke_dict_builder>"
        "-DSOURCE=${PROJECT_SOURCE_DIR}/data/upstream/conway"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/dictionary-cli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/integration/dictionary_cli.cmake")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/compiler_header.cpp" "#include <stroke/build/compiler.hpp>\n")
    add_library(stroke_builder_header_check OBJECT "${CMAKE_CURRENT_BINARY_DIR}/compiler_header.cpp")
    target_link_libraries(stroke_builder_header_check PRIVATE stroke::dictionary_build)
    stroke_configure_target(stroke_builder_header_check)
endif()
