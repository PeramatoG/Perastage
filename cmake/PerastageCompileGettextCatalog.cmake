include("${CMAKE_CURRENT_LIST_DIR}/PerastageValidateCMakeArguments.cmake")

foreach(_required_var IN ITEMS MSGFMT_EXECUTABLE GETTEXT_BIN_DIR INPUT_PO OUTPUT_MO)
    if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
        message(FATAL_ERROR "${_required_var} was not provided.")
    endif()
endforeach()
perastage_reject_quoted_path_values(MSGFMT_EXECUTABLE GETTEXT_BIN_DIR INPUT_PO OUTPUT_MO)


if(NOT EXISTS "${MSGFMT_EXECUTABLE}")
    message(FATAL_ERROR "msgfmt executable was not found: ${MSGFMT_EXECUTABLE}")
endif()

if(NOT EXISTS "${INPUT_PO}")
    message(FATAL_ERROR "Input gettext PO catalog was not found: ${INPUT_PO}")
endif()

get_filename_component(_output_dir "${OUTPUT_MO}" DIRECTORY)
if(_output_dir STREQUAL "")
    message(FATAL_ERROR "Unable to determine output directory for gettext MO catalog: ${OUTPUT_MO}")
endif()

file(MAKE_DIRECTORY "${_output_dir}")
if(NOT IS_DIRECTORY "${_output_dir}")
    message(FATAL_ERROR "Unable to create gettext MO output directory: ${_output_dir}")
endif()

if(WIN32)
    set(_path_separator ";")
else()
    set(_path_separator ":")
endif()

if(EXISTS "${GETTEXT_BIN_DIR}")
    set(ENV{PATH} "${GETTEXT_BIN_DIR}${_path_separator}$ENV{PATH}")
endif()

message(STATUS "Generating gettext catalog: ${INPUT_PO} to ${OUTPUT_MO}")
execute_process(
    COMMAND "${MSGFMT_EXECUTABLE}" --check -o "${OUTPUT_MO}" "${INPUT_PO}"
    RESULT_VARIABLE _msgfmt_result
    OUTPUT_VARIABLE _msgfmt_stdout
    ERROR_VARIABLE _msgfmt_stderr
)

if(NOT _msgfmt_result EQUAL 0)
    message(FATAL_ERROR
        "gettext catalog generation failed with exit code ${_msgfmt_result}.\n"
        "msgfmt: ${MSGFMT_EXECUTABLE}\n"
        "gettext bin dir: ${GETTEXT_BIN_DIR}\n"
        "input PO: ${INPUT_PO}\n"
        "output MO: ${OUTPUT_MO}\n"
        "stdout:\n${_msgfmt_stdout}\n"
        "stderr:\n${_msgfmt_stderr}"
    )
endif()

if(NOT EXISTS "${OUTPUT_MO}")
    message(FATAL_ERROR "msgfmt completed but did not create the expected MO catalog: ${OUTPUT_MO}")
endif()
