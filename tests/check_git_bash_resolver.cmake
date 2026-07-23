cmake_minimum_required(VERSION 3.21)
include("${REPO_ROOT}/cmake/PerastageGitBashResolver.cmake")

set(test_root "${CMAKE_CURRENT_BINARY_DIR}/git-bash-resolver-contract")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}/Git/cmd" "${test_root}/Git/bin" "${test_root}/Windows/System32" "${test_root}/WindowsApps")

set(fake_bash "${test_root}/Git/bin/bash.exe")
file(WRITE "${fake_bash}" "#!/usr/bin/env sh\nif [ \"$1\" = \"--noprofile\" ]; then shift; fi\nif [ \"$1\" = \"--norc\" ]; then shift; fi\nif [ \"$1\" = \"-c\" ]; then shift; eval \"$1\"; exit $?; fi\nexit 2\n")
file(CHMOD "${fake_bash}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(fake_git "${test_root}/Git/cmd/git.exe")
file(WRITE "${fake_git}" "#!/usr/bin/env sh\nprintf 'git version 2.0.0.windows.1\\n'\n")
file(CHMOD "${fake_git}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(system_bash "${test_root}/Windows/System32/bash.exe")
file(WRITE "${system_bash}" "#!/usr/bin/env sh\nexit 1\n")
file(CHMOD "${system_bash}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(windowsapps_bash "${test_root}/WindowsApps/bash.exe")
file(WRITE "${windowsapps_bash}" "#!/usr/bin/env sh\nexit 1\n")
file(CHMOD "${windowsapps_bash}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(BASH_EXECUTABLE "${fake_bash}" CACHE FILEPATH "" FORCE)
perastage_resolve_git_bash(resolved_explicit FORCE_WINDOWS REQUIRED_FOR_TESTING)
if(NOT resolved_explicit STREQUAL fake_bash)
    message(FATAL_ERROR "Explicit Git Bash was not preferred: ${resolved_explicit}")
endif()

perastage_is_rejected_windows_bash_path("C:/Windows/System32/bash.exe" rejected_system32)
if(NOT rejected_system32)
    message(FATAL_ERROR "System32 bash.exe was not rejected.")
endif()
perastage_is_rejected_windows_bash_path("C:/Users/example/AppData/Local/Microsoft/WindowsApps/bash.exe" rejected_windowsapps)
if(NOT rejected_windowsapps)
    message(FATAL_ERROR "WindowsApps bash.exe was not rejected.")
endif()

unset(BASH_EXECUTABLE CACHE)
set(ENV{PATH} "${test_root}/Git/cmd:$ENV{PATH}")
perastage_resolve_git_bash(resolved_auto FORCE_WINDOWS REQUIRED_FOR_TESTING)
if(NOT resolved_auto STREQUAL fake_bash)
    message(FATAL_ERROR "Git-derived Bash was not selected: ${resolved_auto}")
endif()

file(REMOVE "${fake_bash}")
set(missing_failed FALSE)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DREPO_ROOT=${REPO_ROOT} -DTEST_PATH=${test_root}/Git/cmd -P "${REPO_ROOT}/tests/check_git_bash_resolver_missing_case.cmake"
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error)
if(missing_result EQUAL 0)
    message(FATAL_ERROR "Missing Git Bash case unexpectedly passed.")
endif()
if(NOT "${missing_output}${missing_error}" MATCHES "Git Bash could not be resolved")
    message(FATAL_ERROR "Missing Git Bash failure was not actionable: ${missing_output}${missing_error}")
endif()

message("OK: Git Bash resolver contract rejects launchers, prefers explicit Bash, derives from Git for Windows, probes execution, and fails actionably.")
