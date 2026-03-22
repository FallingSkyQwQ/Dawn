cmake_minimum_required(VERSION 3.24)

project(Dawn VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
endif()

option(DAWN_ENABLE_QT "Build the Qt/QML application shell" OFF)
option(DAWN_USE_FLUENTUI "Use the FluentUI submodule when available" ON)
option(DAWN_BUILD_TESTS "Build Dawn tests" ON)
option(DAWN_ENABLE_SQLITE "Enable SQLite support when available" ON)

add_subdirectory(infra)
add_subdirectory(core)

if(DAWN_ENABLE_QT)
    if(NOT DAWN_USE_FLUENTUI)
        message(FATAL_ERROR "DAWN_ENABLE_QT requires DAWN_USE_FLUENTUI=ON. Dawn no longer ships a non-FluentUI Qt shell.")
    endif()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/FluentUI/CMakeLists.txt")
        message(FATAL_ERROR "FluentUI submodule is required for Qt builds. Run: git submodule update --init --recursive external/FluentUI")
    endif()

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
    if(MSVC AND Qt6_DIR MATCHES "mingw")
        message(FATAL_ERROR "Qt6_DIR points to a MinGW Qt kit while the compiler is MSVC. Use an MSVC Qt kit (e.g. .../msvc*/lib/cmake/Qt6).")
    endif()
    if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND Qt6_DIR MATCHES "msvc")
        message(FATAL_ERROR "Qt6_DIR points to an MSVC Qt kit while the compiler is GNU/MinGW. Use a MinGW Qt kit (e.g. .../mingw_64/lib/cmake/Qt6).")
    endif()
    set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "Build FluentUI demo applications." FORCE)
    set(FLUENTUI_QML_PLUGIN_DIRECTORY "${CMAKE_BINARY_DIR}/fluentui_imports/FluentUI" CACHE PATH "Path to FluentUI plugin" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/FluentUI external/FluentUI)

    add_subdirectory(ui)
endif()

add_subdirectory(app)

if(DAWN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
