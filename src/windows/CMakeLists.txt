# Public dependency boundaries remain portable; the concrete TSF adapter uses the Windows SDK.
add_library(stroke_storage_contract INTERFACE)
add_library(stroke::storage_contract ALIAS stroke_storage_contract)
target_include_directories(stroke_storage_contract INTERFACE storage/include)
target_link_libraries(stroke_storage_contract INTERFACE stroke::domain)

add_library(stroke_ui_contract INTERFACE)
add_library(stroke::ui_contract ALIAS stroke_ui_contract)
target_include_directories(stroke_ui_contract INTERFACE ui/include)
target_link_libraries(stroke_ui_contract INTERFACE stroke::engine)

add_library(stroke_tsf_contract INTERFACE)
add_library(stroke::tsf_contract ALIAS stroke_tsf_contract)
target_include_directories(stroke_tsf_contract INTERFACE tsf/include)
target_link_libraries(stroke_tsf_contract INTERFACE
    stroke::engine stroke::ui_contract stroke::storage_contract)

if(WIN32 AND MSVC)
    enable_language(RC)
    add_library(stroke_layout_store STATIC storage/layout_store.cpp storage/learning_store.cpp)
    target_include_directories(stroke_layout_store PUBLIC storage)
    target_compile_definitions(stroke_layout_store PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(stroke_layout_store PUBLIC stroke::domain PRIVATE shell32 ole32)
    stroke_configure_target(stroke_layout_store)
    # The x64 WinUI settings executable has its own app-local runtime and MSBuild
    # project; do not link Windows App SDK into the in-process TSF DLL.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_custom_target(stroke_settings
            COMMAND pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/apps/settings/winui/build.ps1"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)
    endif()
    add_library(stroke_tsf SHARED tsf/module.cpp tsf/service.cpp tsf/registration.cpp
        ui/candidate_window.cpp tsf/stroke_tsf.def tsf/resources/stroke.rc)
    target_include_directories(stroke_tsf PRIVATE tsf/resources)
    target_compile_definitions(stroke_tsf PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(stroke_tsf PRIVATE stroke::engine stroke_layout_store ole32 oleaut32 uuid user32 gdi32 advapi32)
    stroke_configure_target(stroke_tsf)
    add_custom_command(TARGET stroke_tsf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/generated/conway-v2.0.2-traditional"
            "$<TARGET_FILE_DIR:stroke_tsf>/dictionary")
    add_executable(stroke_setup "${PROJECT_SOURCE_DIR}/apps/setup/main.cpp")
    target_include_directories(stroke_setup PRIVATE tsf)
    target_compile_definitions(stroke_setup PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(stroke_setup PRIVATE ole32 uuid user32)
    stroke_configure_target(stroke_setup)
    if(BUILD_TESTING)
        add_test(NAME windows.com_contracts COMMAND stroke_setup selftest "$<TARGET_FILE:stroke_tsf>")
    endif()
endif()
