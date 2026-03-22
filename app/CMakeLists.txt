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
    target_link_libraries(dawn PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    if(TARGET dawn_ui)
        target_link_libraries(dawn PRIVATE dawn_ui)
    endif()

    if(DAWN_USE_FLUENTUIBI AND EXISTS "${CMAKE_SOURCE_DIR}/external/FluentUIbi/CMakeLists.txt")
        add_subdirectory(${CMAKE_SOURCE_DIR}/external/FluentUIbi external/FluentUIbi)
        target_compile_definitions(dawn PRIVATE DAWN_USE_FLUENTUIBI=1)
    endif()
endif()
