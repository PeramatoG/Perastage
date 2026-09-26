add_library(perastage_cli_support STATIC)
target_sources(perastage_cli_support PRIVATE cli_runner.cpp cli_runner.h)
target_compile_features(perastage_cli_support PUBLIC cxx_std_20)
target_include_directories(perastage_cli_support
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_SOURCE_DIR}/core")
target_compile_definitions(perastage_cli_support PRIVATE
    PERASTAGE_CLI_VERSION="${PROJECT_VERSION}")

add_executable(perastage_cli)
target_sources(perastage_cli PRIVATE main.cpp)
target_compile_features(perastage_cli PRIVATE cxx_std_20)
target_link_libraries(perastage_cli PRIVATE perastage_cli_support)
set_target_properties(perastage_cli PROPERTIES OUTPUT_NAME "perastage-cli")

if(BUILD_TESTING)
    add_executable(perastage_cli_runner_test
        "${CMAKE_SOURCE_DIR}/tests/perastage_cli_runner_test.cpp")
    target_link_libraries(perastage_cli_runner_test PRIVATE perastage_cli_support)
    target_compile_definitions(perastage_cli_runner_test PRIVATE
        PERASTAGE_CLI_TEST_VERSION="${PROJECT_VERSION}")
    add_test(NAME PerastageCliRunner COMMAND perastage_cli_runner_test)
    set_tests_properties(PerastageCliRunner PROPERTIES
        LABELS "standard-strict;architecture;cli")

    add_test(NAME PerastageCliSmoke
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/perastage_cli_smoke_test.py"
                "$<TARGET_FILE:perastage_cli>" "${PROJECT_VERSION}")
    set_tests_properties(PerastageCliSmoke PROPERTIES
        LABELS "standard-strict;architecture;cli" TIMEOUT 20)
endif()
