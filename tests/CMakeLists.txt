find_package(GTest QUIET)

if(GTest_FOUND)
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
else()
    message(WARNING "GoogleTest was not found. Dawn test targets are skipped.")
endif()
