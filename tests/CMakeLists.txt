find_package(GTest QUIET)

if(NOT TARGET GTest::gtest)
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT for GoogleTest" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(TARGET GTest::gtest)
    add_executable(dawn_tests)

    target_sources(dawn_tests
        PRIVATE
            account_token_state_test.cpp
            backup_service_test.cpp
            content_install_service_test.cpp
            diagnostics_service_test.cpp
            download_service_test.cpp
            global_settings_test.cpp
            hash_test.cpp
            http_client_factory_test.cpp
            instance_manifest_test.cpp
            microsoft_identity_service_test.cpp
            microsoft_oauth_service_test.cpp
            modrinth_provider_protocol_test.cpp
            preflight_test.cpp
            task_pipeline_test.cpp
    )

    target_link_libraries(dawn_tests
        PRIVATE
            dawn_core
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME dawn_tests COMMAND dawn_tests)

    if(TARGET dawn_ui)
        add_executable(dawn_ui_tests)

        target_sources(dawn_ui_tests
            PRIVATE
                app_view_model_test.cpp
        )

        target_link_libraries(dawn_ui_tests
            PRIVATE
                dawn_ui
                GTest::gtest
                Qt6::Core
        )

        add_test(NAME dawn_ui_tests COMMAND dawn_ui_tests)
    endif()
else()
    message(FATAL_ERROR "GoogleTest could not be configured.")
endif()
