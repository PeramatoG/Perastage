include("${REPO_ROOT}/cmake/PerastageWindowsVcpkgResolver.cmake")

# Fails with native CMake path diagnostics when two roots are not equivalent.
function(assert_same_vcpkg_root expected_root actual_root assertion_name)
    _perastage_normalize_vcpkg_root("${expected_root}" normalized_expected_root)
    _perastage_normalize_vcpkg_root("${actual_root}" normalized_actual_root)
    if(NOT normalized_actual_root STREQUAL normalized_expected_root)
        message(FATAL_ERROR
            "${CASE} ${assertion_name} failed: expected '${normalized_expected_root}', actual '${normalized_actual_root}'."
        )
    endif()
endfunction()

unset(ENV{VCPKG_ROOT})
unset(ENV{LOCALAPPDATA})
unset(ENV{APPDATA})

if(CASE STREQUAL "explicit-external")
    set(ENV{VCPKG_ROOT} "${EXTERNAL_ROOT}")
    set(ENV{LOCALAPPDATA} "${USER_CONFIG_ROOT}")
elseif(CASE STREQUAL "visual-studio-override")
    set(ENV{VCPKG_ROOT} "C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg")
    set(ENV{LOCALAPPDATA} "${USER_CONFIG_ROOT}")
elseif(CASE STREQUAL "user-wide-only" OR CASE STREQUAL "bootstrap-user-wide")
    set(ENV{LOCALAPPDATA} "${USER_CONFIG_ROOT}")
elseif(CASE STREQUAL "appdata-only")
    set(ENV{APPDATA} "${USER_CONFIG_ROOT}")
elseif(CASE STREQUAL "stale-descriptor")
    set(ENV{LOCALAPPDATA} "${STALE_CONFIG_ROOT}")
elseif(CASE STREQUAL "bundled-descriptor")
    set(ENV{LOCALAPPDATA} "${BUNDLED_CONFIG_ROOT}")
elseif(NOT CASE STREQUAL "both-missing")
    message(FATAL_ERROR "Unknown Windows vcpkg resolver fixture case: ${CASE}")
endif()

if(CASE STREQUAL "bootstrap-user-wide")
    include("${REPO_ROOT}/cmake/PerastageWindowsVcpkgToolchain.cmake")
    assert_same_vcpkg_root("${EXTERNAL_ROOT}" "${PERASTAGE_RESOLVED_VCPKG_ROOT}"
        "resolved-root assertion")
    assert_same_vcpkg_root("${EXTERNAL_ROOT}" "$ENV{VCPKG_ROOT}"
        "environment-root assertion")
    if(NOT SYNTHETIC_TOOLCHAIN_INCLUDED)
        message(FATAL_ERROR
            "${CASE} toolchain assertion failed: the synthetic external toolchain was not included."
        )
    endif()
else()
    perastage_resolve_windows_vcpkg_root(resolved_root)
    assert_same_vcpkg_root("${EXTERNAL_ROOT}" "${resolved_root}" "resolved-root assertion")
endif()

file(WRITE "${RESULT_FILE}" "PASS")
