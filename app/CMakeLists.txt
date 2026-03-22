add_executable(dawn)

target_sources(dawn
    PRIVATE
        main.cpp
)

target_include_directories(dawn
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/infra/include
)

target_link_libraries(dawn
    PRIVATE
        dawn_core
)

target_compile_definitions(dawn PRIVATE DAWN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(DAWN_ENABLE_QT)
    target_include_directories(dawn PRIVATE ${CMAKE_SOURCE_DIR}/ui/viewmodels)
    target_compile_definitions(dawn PRIVATE DAWN_ENABLE_QT=1)
    target_link_libraries(dawn PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia)
    if(TARGET dawn_ui)
        target_link_libraries(dawn PRIVATE dawn_ui)
    endif()

    if(NOT TARGET fluentuiplugin)
        message(FATAL_ERROR "fluentuiplugin target is required for Dawn Qt application.")
    endif()
    target_compile_definitions(dawn
        PRIVATE
            DAWN_USE_FLUENTUI=1
            DAWN_FLUENTUI_IMPORT_PATH="${FLUENTUI_QML_PLUGIN_DIRECTORY}"
    )
    add_dependencies(dawn fluentuiplugin)
endif()
