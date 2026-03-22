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
    set(_dawn_test_runtime_path "")
    if(WIN32)
        if(CMAKE_CXX_COMPILER)
            get_filename_component(_dawn_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            if(EXISTS "${_dawn_compiler_dir}/libstdc++-6.dll")
                set(_dawn_test_runtime_path "${_dawn_compiler_dir}")
            endif()
        endif()

        if(_dawn_test_runtime_path STREQUAL "" AND EXISTS "C:/Qt/6.10.2/mingw_64/bin/libstdc++-6.dll")
            set(_dawn_test_runtime_path "C:/Qt/6.10.2/mingw_64/bin")
        endif()
    endif()

    function(dawn_add_test_with_runtime_path test_name target_name)
        if(WIN32 AND NOT _dawn_test_runtime_path STREQUAL "")
            set(_dawn_test_path_env "PATH=${_dawn_test_runtime_path};$ENV{PATH}")
            add_test(NAME "${test_name}" COMMAND "${CMAKE_COMMAND}" -E env "${_dawn_test_path_env}" $<TARGET_FILE:${target_name}>)
        else()
            add_test(NAME "${test_name}" COMMAND $<TARGET_FILE:${target_name}>)
        endif()
    endfunction()

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

    dawn_add_test_with_runtime_path(dawn_tests dawn_tests)

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

        dawn_add_test_with_runtime_path(dawn_ui_tests dawn_ui_tests)
    endif()
else()
    message(FATAL_ERROR "GoogleTest could not be configured.")
endif()
