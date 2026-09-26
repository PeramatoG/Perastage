add_library(perastage_cli_support STATIC)
target_sources(perastage_cli_support PRIVATE
    cli_runner.cpp cli_runner.h
    inspect_command.cpp inspect_command.h
    inspection_outcome.cpp inspection_outcome.h
    inspection_text_formatter.cpp inspection_text_formatter.h)
target_compile_features(perastage_cli_support PUBLIC cxx_std_20)
target_include_directories(perastage_cli_support
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_SOURCE_DIR}/core")
target_compile_definitions(perastage_cli_support PRIVATE
    PERASTAGE_CLI_VERSION="${PROJECT_VERSION}")
target_link_libraries(perastage_cli_support PUBLIC
    perastage_inspection_gdtf
    perastage_inspection_mvr
    perastage_inspection_resource
    perastage_inspection_serialization)

add_executable(perastage_cli)
target_sources(perastage_cli PRIVATE main.cpp)
target_compile_features(perastage_cli PRIVATE cxx_std_20)
target_link_libraries(perastage_cli PRIVATE perastage_cli_support)
set_target_properties(perastage_cli PROPERTIES OUTPUT_NAME "perastage-cli")
