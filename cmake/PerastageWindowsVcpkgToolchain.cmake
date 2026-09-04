include("${CMAKE_CURRENT_LIST_DIR}/PerastageWindowsVcpkgResolver.cmake")

perastage_resolve_windows_vcpkg_root(resolved_vcpkg_root)
set(ENV{VCPKG_ROOT} "${resolved_vcpkg_root}")
set(PERASTAGE_RESOLVED_VCPKG_ROOT "${resolved_vcpkg_root}" CACHE INTERNAL
    "External classic vcpkg checkout selected by Perastage" FORCE)
message(STATUS "Perastage external classic vcpkg root: ${resolved_vcpkg_root}")

set(resolved_vcpkg_toolchain "${resolved_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
include("${resolved_vcpkg_toolchain}")
