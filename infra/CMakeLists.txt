add_library(dawn_infra STATIC)

target_sources(dawn_infra
    PRIVATE
        src/fs/file_system.cpp
        src/json/simple_json.cpp
)

target_include_directories(dawn_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dawn::infra ALIAS dawn_infra)
