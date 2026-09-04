# Normalizes a Windows path for reliable case-insensitive comparisons.
function(_perastage_normalize_windows_path input_path output_variable)
    file(TO_CMAKE_PATH "${input_path}" normalized_path)
    cmake_path(NORMAL_PATH normalized_path)
    string(TOLOWER "${normalized_path}" normalized_path)
    set(${output_variable} "${normalized_path}" PARENT_SCOPE)
endfunction()

# Rejects invalid toolchain selection for the supported local Windows classic-vcpkg workflow.
function(perastage_validate_windows_classic_vcpkg host_is_windows)
    if(NOT host_is_windows)
        return()
    endif()

    _perastage_normalize_windows_path("${CMAKE_TOOLCHAIN_FILE}" normalized_toolchain)
    if(normalized_toolchain MATCHES "/microsoft visual studio/[^/]+/[^/]+/vc/vcpkg/scripts/buildsystems/vcpkg\\.cmake$")
        message(FATAL_ERROR
            "Perastage local Windows builds intentionally use the external classic vcpkg checkout selected by VCPKG_ROOT; Visual Studio's bundled VC/vcpkg checkout is not supported for this workflow. Select win-x64-debug-ninja or win-x64-release-ninja and set VCPKG_ROOT to the existing classic checkout, either in the Visual Studio process environment or an ignored CMakeUserPresets.json environment map. Restart Visual Studio or reopen the folder after changing a persistent environment variable, then clear and reconfigure the affected CMake cache."
        )
    endif()

    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows" OR
       NOT VCPKG_MANIFEST_MODE STREQUAL "OFF" OR
       NOT VCPKG_MANIFEST_INSTALL STREQUAL "OFF")
        return()
    endif()

    if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
        message(FATAL_ERROR
            "The Perastage Windows classic-vcpkg preset requires VCPKG_ROOT. Set it to the existing classic vcpkg checkout in the Visual Studio process environment or an ignored CMakeUserPresets.json environment map, restart or reopen Visual Studio if needed, then clear and reconfigure the affected CMake cache."
        )
    endif()

    set(expected_toolchain "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    if(NOT EXISTS "${expected_toolchain}")
        message(FATAL_ERROR
            "The Perastage Windows classic-vcpkg preset could not find ${expected_toolchain}. Correct VCPKG_ROOT to the existing classic vcpkg checkout, then clear and reconfigure the affected CMake cache."
        )
    endif()

    _perastage_normalize_windows_path("${expected_toolchain}" normalized_expected_toolchain)
    if(NOT normalized_toolchain STREQUAL normalized_expected_toolchain)
        message(FATAL_ERROR
            "The selected CMake toolchain does not match the classic vcpkg checkout in VCPKG_ROOT. Select win-x64-debug-ninja or win-x64-release-ninja, correct VCPKG_ROOT in the Visual Studio environment or ignored CMakeUserPresets.json, then clear and reconfigure the affected CMake cache."
        )
    endif()
endfunction()
