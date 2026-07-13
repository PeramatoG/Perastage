include("${CMAKE_CURRENT_LIST_DIR}/PerastageValidateCMakeArguments.cmake")

if(NOT DEFINED EXPECTED_FILE OR EXPECTED_FILE STREQUAL "")
    message(FATAL_ERROR "EXPECTED_FILE was not provided.")
endif()
perastage_reject_quoted_path_value(EXPECTED_FILE)
if(NOT EXISTS "${EXPECTED_FILE}")
    message(FATAL_ERROR "Expected generated localization catalog was not created: ${EXPECTED_FILE}")
endif()
