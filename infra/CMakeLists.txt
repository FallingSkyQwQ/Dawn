add_library(dawn_infra STATIC)

target_sources(dawn_infra
    PRIVATE
        src/fs/file_system.cpp
        src/json/simple_json.cpp
        src/net/http_client_factory.cpp
)

if(WIN32)
    target_sources(dawn_infra PRIVATE src/net/win_http_client.cpp)
    target_link_libraries(dawn_infra PUBLIC winhttp)
endif()

target_include_directories(dawn_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dawn::infra ALIAS dawn_infra)
