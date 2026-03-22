add_library(dawn_core STATIC)

target_sources(dawn_core
    PRIVATE
        src/auth/account_service.cpp
        src/auth/microsoft_oauth_service.cpp
        src/backup/backup_service.cpp
        src/diagnostics/diagnostics_service.cpp
        src/download/download_service.cpp
        src/java/java_service.cpp
        src/loaders/loader_service.cpp
        src/model/content_lock.cpp
        src/model/instance_workbench.cpp
        src/model/instance_manifest.cpp
        src/model/launch_types.cpp
        src/model/preflight.cpp
        src/model/task_types.cpp
        src/model/content_types.cpp
        src/minecraft/minecraft_service.cpp
        src/serialization/manifest_codec.cpp
        src/interfaces/default_launcher_runtime.cpp
        src/pipeline/task_pipeline.cpp
        src/service/instance_service.cpp
        src/service/preflight_service.cpp
        src/service/task_queue.cpp
        src/provider/modrinth_provider.cpp
        src/provider/optifine_adapter.cpp
        src/settings/settings_service.cpp
        src/storage/instance_repository.cpp
        src/storage/sqlite_database.cpp
        src/storage/task_history_repository.cpp
)

target_include_directories(dawn_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dawn_core
    PUBLIC
        dawn_infra
)

if(DAWN_ENABLE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        target_compile_definitions(dawn_core PUBLIC DAWN_HAS_SQLITE3=1)
        target_link_libraries(dawn_core PUBLIC SQLite::SQLite3)
    endif()
endif()

add_library(dawn::core ALIAS dawn_core)
