# Normalizes a candidate path for stable Windows comparisons.
function(_perastage_normalize_vcpkg_root candidate output_variable)
    file(TO_CMAKE_PATH "${candidate}" normalized_candidate)
    cmake_path(NORMAL_PATH normalized_candidate)
    set(${output_variable} "${normalized_candidate}" PARENT_SCOPE)
endfunction()

# Reports whether a root has the semantic shape of Visual Studio's bundled vcpkg.
function(perastage_is_visual_studio_vcpkg_root candidate output_variable)
    _perastage_normalize_vcpkg_root("${candidate}" normalized_candidate)
    string(TOLOWER "${normalized_candidate}" normalized_candidate_lower)
    if(normalized_candidate_lower MATCHES "/microsoft visual studio/[^/]+/[^/]+/vc/vcpkg/?$")
        set(${output_variable} TRUE PARENT_SCOPE)
    else()
        set(${output_variable} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Validates the minimum files that identify an external classic vcpkg checkout.
function(perastage_validate_external_vcpkg_root candidate valid_variable root_variable)
    _perastage_normalize_vcpkg_root("${candidate}" normalized_candidate)
    perastage_is_visual_studio_vcpkg_root("${normalized_candidate}" is_visual_studio_root)
    if(normalized_candidate STREQUAL "" OR is_visual_studio_root OR
       NOT EXISTS "${normalized_candidate}/.vcpkg-root" OR
       NOT EXISTS "${normalized_candidate}/vcpkg.exe" OR
       NOT EXISTS "${normalized_candidate}/scripts/buildsystems/vcpkg.cmake")
        set(${valid_variable} FALSE PARENT_SCOPE)
        set(${root_variable} "" PARENT_SCOPE)
        return()
    endif()
    set(${valid_variable} TRUE PARENT_SCOPE)
    set(${root_variable} "${normalized_candidate}" PARENT_SCOPE)
endfunction()

# Selects an external classic checkout from the environment or user-wide integration.
function(perastage_resolve_windows_vcpkg_root output_variable)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
        perastage_validate_external_vcpkg_root("$ENV{VCPKG_ROOT}" environment_valid environment_root)
        if(environment_valid)
            set(${output_variable} "${environment_root}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(user_config_root "")
    if(DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
        set(user_config_root "$ENV{LOCALAPPDATA}")
    elseif(DEFINED ENV{APPDATA} AND NOT "$ENV{APPDATA}" STREQUAL "")
        set(user_config_root "$ENV{APPDATA}")
    endif()

    set(integration_descriptor "${user_config_root}/vcpkg/vcpkg.path.txt")
    if(NOT user_config_root STREQUAL "" AND EXISTS "${integration_descriptor}")
        file(READ "${integration_descriptor}" descriptor_root)
        string(STRIP "${descriptor_root}" descriptor_root)
        perastage_validate_external_vcpkg_root("${descriptor_root}" descriptor_valid descriptor_normalized_root)
        if(descriptor_valid)
            set(${output_variable} "${descriptor_normalized_root}" PARENT_SCOPE)
            return()
        endif()
    endif()

    message(FATAL_ERROR
        "Perastage could not resolve an external classic vcpkg checkout. Define VCPKG_ROOT in an ignored CMakeUserPresets.json environment map, or run <external-vcpkg-root>\\vcpkg.exe integrate install once from the intended checkout to register it in the standard user-wide vcpkg configuration. Visual Studio's bundled VC/vcpkg checkout is deliberately ignored. Then reopen Visual Studio and reconfigure."
    )
endfunction()
