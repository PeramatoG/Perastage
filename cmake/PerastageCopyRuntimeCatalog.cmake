include("${CMAKE_CURRENT_LIST_DIR}/PerastageValidateCMakeArguments.cmake")

foreach(_required_var IN ITEMS SOURCE_MO DESTINATION_MO)
    if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
        message(FATAL_ERROR "${_required_var} was not provided.")
    endif()
endforeach()
perastage_reject_quoted_path_values(SOURCE_MO DESTINATION_MO)


if(NOT EXISTS "${SOURCE_MO}")
    message(FATAL_ERROR "Runtime catalog source does not exist: ${SOURCE_MO}")
endif()

get_filename_component(_destination_dir "${DESTINATION_MO}" DIRECTORY)
if(_destination_dir STREQUAL "")
    message(FATAL_ERROR "Unable to determine runtime catalog destination directory: ${DESTINATION_MO}")
endif()

file(MAKE_DIRECTORY "${_destination_dir}")
file(COPY_FILE "${SOURCE_MO}" "${DESTINATION_MO}" ONLY_IF_DIFFERENT)

if(NOT EXISTS "${DESTINATION_MO}")
    message(FATAL_ERROR "Runtime catalog copy did not create the expected file: ${DESTINATION_MO}")
endif()

message(STATUS "Copied runtime catalog: ${SOURCE_MO} to ${DESTINATION_MO}")
