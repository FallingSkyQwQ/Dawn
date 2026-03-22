add_library(dawn_ui STATIC)

target_sources(dawn_ui
    PRIVATE
        viewmodels/app_view_model.cpp
)

target_include_directories(dawn_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/infra/include
)

target_link_libraries(dawn_ui
    PUBLIC
        dawn_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

if(DAWN_USE_FLUENTUIBI AND EXISTS "${CMAKE_SOURCE_DIR}/external/FluentUIbi/CMakeLists.txt")
    target_compile_definitions(dawn_ui PUBLIC DAWN_HAS_FLUENTUIBI=1)
endif()

add_library(dawn::ui ALIAS dawn_ui)
