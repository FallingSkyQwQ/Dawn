cmake_minimum_required(VERSION 3.24)

project(Dawn VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DAWN_ENABLE_QT "Build the Qt/QML application shell" OFF)
option(DAWN_USE_FLUENTUIBI "Use the FluentUIbi submodule when available" ON)
option(DAWN_BUILD_TESTS "Build Dawn tests" ON)
option(DAWN_ENABLE_SQLITE "Enable SQLite support when available" ON)

add_subdirectory(infra)
add_subdirectory(core)

if(DAWN_ENABLE_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
    add_subdirectory(ui)
endif()

add_subdirectory(app)

if(DAWN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
