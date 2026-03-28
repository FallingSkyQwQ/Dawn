add_library(dawn_infra STATIC)

find_package(CURL QUIET)

target_sources(dawn_infra
    PRIVATE
        src/archive/zip_archive.cpp
        src/fs/file_system.cpp
        src/hash/sha256.cpp
        src/json/simple_json.cpp
        src/net/http_client_factory.cpp
)

if(WIN32)
    target_sources(dawn_infra PRIVATE src/net/win_http_client.cpp)
    target_link_libraries(dawn_infra PUBLIC winhttp)
endif()

if(CURL_FOUND)
    target_sources(dawn_infra PRIVATE src/net/curl_http_client.cpp)
    target_link_libraries(dawn_infra PUBLIC CURL::libcurl)
    target_compile_definitions(dawn_infra PUBLIC DAWN_HAS_CURL=1)
elseif(WIN32)
    message(STATUS "libcurl not found; using WinHTTP as the default transport on Windows.")
else()
    message(FATAL_ERROR "libcurl is required on non-Windows platforms. Install libcurl development headers/libraries.")
endif()

target_include_directories(dawn_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dawn::infra ALIAS dawn_infra)
