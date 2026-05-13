set(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../..")
set(_version_file "${_repo_root}/VERSION")

if(NOT EXISTS "${_version_file}")
  message(FATAL_ERROR "VERSION file not found at ${_version_file}")
endif()

file(READ "${_version_file}" _project_version_raw)
string(STRIP "${_project_version_raw}" _project_version)

if(NOT _project_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR
    "Invalid version '${_project_version}' in ${_version_file}. Expected MAJOR.MINOR.PATCH")
endif()

message(STATUS "${_project_version}")
