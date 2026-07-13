include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PerastageValidateCMakeArguments.cmake")

foreach(_required_var IN ITEMS HELPER_SCRIPT MSGFMT_EXECUTABLE GETTEXT_BIN_DIR INPUT_PO TEST_ROOT)
    if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
        message(FATAL_ERROR "${_required_var} was not provided.")
    endif()
endforeach()
perastage_reject_quoted_path_values(HELPER_SCRIPT MSGFMT_EXECUTABLE GETTEXT_BIN_DIR INPUT_PO TEST_ROOT)


set(_space_root "${TEST_ROOT}/gettext helper path with spaces")
set(_input_dir "${_space_root}/input/es/LC_MESSAGES")
set(_output_mo "${_space_root}/output/es/LC_MESSAGES/perastage.mo")
file(REMOVE_RECURSE "${_space_root}")
file(MAKE_DIRECTORY "${_input_dir}")
file(COPY_FILE "${INPUT_PO}" "${_input_dir}/perastage.po")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DMSGFMT_EXECUTABLE=${MSGFMT_EXECUTABLE}"
            "-DGETTEXT_BIN_DIR=${GETTEXT_BIN_DIR}"
            "-DINPUT_PO=${_input_dir}/perastage.po"
            "-DOUTPUT_MO=${_output_mo}"
            -P "${HELPER_SCRIPT}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Gettext helper regression failed.\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

if(NOT EXISTS "${_output_mo}")
    message(FATAL_ERROR "Gettext helper regression did not create ${_output_mo}")
endif()
