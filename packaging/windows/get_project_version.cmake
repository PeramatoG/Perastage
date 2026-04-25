set(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../..")
set(_cmake_lists "${_repo_root}/CMakeLists.txt")

if(NOT EXISTS "${_cmake_lists}")
  message(FATAL_ERROR "CMakeLists.txt not found at ${_cmake_lists}")
endif()

file(STRINGS "${_cmake_lists}" _project_line
     REGEX "^project\\(Perastage VERSION [0-9]+\\.[0-9]+\\.[0-9]+")

if(NOT _project_line)
  message(FATAL_ERROR "Could not find Perastage project version in ${_cmake_lists}")
endif()

list(GET _project_line 0 _project_line_value)
string(REGEX REPLACE "^project\\(Perastage VERSION ([0-9]+\\.[0-9]+\\.[0-9]+).*$"
                     "\\1"
                     _project_version
                     "${_project_line_value}")

if(NOT _project_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "Extracted invalid project version: '${_project_version}'")
endif()

message(STATUS "${_project_version}")
