cmake_minimum_required(VERSION 3.21)
include("${REPO_ROOT}/cmake/PerastageGitBashResolver.cmake")

set(test_root "${CMAKE_CURRENT_BINARY_DIR}/git-bash-resolver-contract")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}/Host Bash With Spaces" "${test_root}/Git/cmd")

perastage_is_rejected_windows_bash_path("C:/Windows/System32/bash.exe" rejected_system32)
if(NOT rejected_system32)
    message(FATAL_ERROR "System32 bash.exe was not rejected.")
endif()
perastage_is_rejected_windows_bash_path("C:/Users/example/AppData/Local/Microsoft/WindowsApps/bash.exe" rejected_windowsapps)
if(NOT rejected_windowsapps)
    message(FATAL_ERROR "WindowsApps bash.exe was not rejected.")
endif()

perastage_git_for_windows_bash_candidates("C:/Program Files/Git/cmd/git.exe" derived_candidates)
if(NOT "C:/Program Files/Git/bin/bash.exe" IN_LIST derived_candidates)
    message(FATAL_ERROR "Git-for-Windows bin bash candidate was not derived: ${derived_candidates}")
endif()
if(NOT "C:/Program Files/Git/usr/bin/bash.exe" IN_LIST derived_candidates)
    message(FATAL_ERROR "Git-for-Windows usr/bin bash candidate was not derived: ${derived_candidates}")
endif()

set(host_bash "")
if(WIN32)
    if(NOT DEFINED BASH_EXECUTABLE OR "${BASH_EXECUTABLE}" STREQUAL "")
        message(FATAL_ERROR "Native Windows resolver contract requires the real CMake-resolved Git Bash path in BASH_EXECUTABLE.")
    endif()
    set(host_bash "${BASH_EXECUTABLE}")
else()
    set(host_bash "${test_root}/Host Bash With Spaces/bash")
    file(WRITE "${host_bash}" "#!/usr/bin/env sh\nif [ \"$1\" = \"--noprofile\" ]; then shift; fi\nif [ \"$1\" = \"--norc\" ]; then shift; fi\nif [ \"$1\" = \"-c\" ]; then shift; eval \"$1\"; exit $?; fi\nexit 2\n")
    file(CHMOD "${host_bash}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

set(BASH_EXECUTABLE "${host_bash}" CACHE FILEPATH "" FORCE)
perastage_resolve_git_bash(resolved_explicit REQUIRED_FOR_TESTING)
if(NOT resolved_explicit STREQUAL host_bash)
    message(FATAL_ERROR "Explicit Bash was not preferred: ${resolved_explicit}")
endif()

unset(BASH_EXECUTABLE CACHE)
unset(BASH_EXECUTABLE)
if(WIN32)
    perastage_resolve_git_bash(resolved_auto FORCE_WINDOWS REQUIRED_FOR_TESTING)
    perastage_validate_bash_probe("${resolved_auto}" auto_probe_ok auto_probe_message)
    if(NOT auto_probe_ok)
        message(FATAL_ERROR "Automatically derived Git Bash failed probe: ${auto_probe_message}")
    endif()
else()
    set(ENV{PATH} "${test_root}/Host Bash With Spaces:$ENV{PATH}")
    perastage_resolve_git_bash(resolved_auto REQUIRED_FOR_TESTING)
    if(NOT resolved_auto STREQUAL host_bash)
        message(FATAL_ERROR "Non-Windows PATH discovery did not select the test Bash with spaces: ${resolved_auto}")
    endif()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -DREPO_ROOT=${REPO_ROOT} -DCASE_MODE=invalid-explicit -P "${REPO_ROOT}/tests/check_git_bash_resolver_missing_case.cmake"
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_output
    ERROR_VARIABLE invalid_error)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "Invalid explicit Bash case unexpectedly passed.")
endif()
if(NOT "${invalid_output}${invalid_error}" MATCHES "not usable Git Bash|last probe")
    message(FATAL_ERROR "Invalid explicit Bash failure was not actionable: ${invalid_output}${invalid_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -DREPO_ROOT=${REPO_ROOT} -DCASE_MODE=missing -P "${REPO_ROOT}/tests/check_git_bash_resolver_missing_case.cmake"
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error)
if(missing_result EQUAL 0)
    message(FATAL_ERROR "Missing Bash case unexpectedly passed.")
endif()
if(WIN32)
    set(expected_missing "Git Bash could not be resolved")
else()
    set(expected_missing "Bash is required for shell policy tests")
endif()
if(NOT "${missing_output}${missing_error}" MATCHES "${expected_missing}")
    message(FATAL_ERROR "Missing Bash failure was not actionable: ${missing_output}${missing_error}")
endif()

message("OK: Bash resolver contract covers explicit precedence, host PATH discovery, Windows path rejection, Git candidate derivation, probes, and actionable failures.")
