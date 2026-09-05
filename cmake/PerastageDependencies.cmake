# Discover application dependencies and validate required package capabilities.

# wxWidgets chooses different binary sets based on wxWidgets_USE_DEBUG.
# - Single-config: we can select debug libs based on CMAKE_BUILD_TYPE.
# - Multi-config (Visual Studio): do not force wxWidgets_USE_DEBUG unless explicitly requested.
if(PERASTAGE_FORCE_WXDEBUG)
    set(wxWidgets_USE_DEBUG ON)
elseif(NOT CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_USE_DEBUG ON)
    else()
        set(wxWidgets_USE_DEBUG OFF)
    endif()
endif()
set(wxWidgets_USE_UNICODE ON)

if(WIN32)
    find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base aui gl html richtext xml)
    set(_wx_libs
        wx::core
        wx::base
        wx::aui
        wx::gl
        wx::html
        wx::richtext
    )
    if(TARGET wx::xml)
        list(APPEND _wx_libs wx::xml)
    endif()
    set(_wx_includes "")
    find_package(tinyxml2 CONFIG REQUIRED)
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html richtext xml)
    include(${wxWidgets_USE_FILE})
    set(_wx_libs ${wxWidgets_LIBRARIES})
    set(_wx_includes ${wxWidgets_INCLUDE_DIRS})
    find_package(tinyxml2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(GLEW REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
if(WIN32)
    find_package(nanovg CONFIG REQUIRED)
    find_package(podofo CONFIG REQUIRED)
else()
    find_package(nanovg CONFIG QUIET)
    if(NOT nanovg_FOUND)
        find_path(NANOVG_INCLUDE_DIR nanovg.h)
        find_library(NANOVG_LIBRARY nanovg)
        if(NOT NANOVG_INCLUDE_DIR OR NOT NANOVG_LIBRARY)
            message(FATAL_ERROR "nanovg not found. Install libnanovg-dev or provide nanovgConfig.cmake.")
        endif()
        add_library(nanovg::nanovg UNKNOWN IMPORTED)
        set_target_properties(nanovg::nanovg PROPERTIES
            IMPORTED_LOCATION "${NANOVG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NANOVG_INCLUDE_DIR}"
        )
    endif()

    find_package(podofo CONFIG QUIET)
    if(NOT podofo_FOUND)
        find_path(PODOFO_INCLUDE_DIR podofo/podofo.h)
        find_library(PODOFO_LIBRARY podofo)
        if(NOT PODOFO_INCLUDE_DIR OR NOT PODOFO_LIBRARY)
            message(FATAL_ERROR "podofo not found. Install libpodofo-dev or provide podofoConfig.cmake.")
        endif()
        add_library(podofo::podofo UNKNOWN IMPORTED)
        set_target_properties(podofo::podofo PROPERTIES
            IMPORTED_LOCATION "${PODOFO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PODOFO_INCLUDE_DIR}"
        )
    endif()
endif()
find_package(ZLIB REQUIRED)
find_package(Backward CONFIG REQUIRED)

if(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS)
    find_package(mdns CONFIG REQUIRED)
    message(STATUS "MVR-xchange mDNS backend enabled: vcpkg mdns")
else()
    message(WARNING "MVR-xchange mDNS backend disabled by PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF.")
endif()

# Verifies that the configured wxWidgets build exposes wxSecretStore support.
function(perastage_probe_wx_secretstore out_var)
    include(CheckCXXSourceCompiles)
    set(_previous_required_includes "${CMAKE_REQUIRED_INCLUDES}")
    set(_probe_includes ${_wx_includes})
    foreach(_wx_target wx::base wx::core)
        if(TARGET ${_wx_target})
            get_target_property(_target_includes ${_wx_target} INTERFACE_INCLUDE_DIRECTORIES)
            if(_target_includes)
                list(APPEND _probe_includes ${_target_includes})
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _probe_includes)
    set(CMAKE_REQUIRED_INCLUDES ${_probe_includes})
    check_cxx_source_compiles("#include <wx/setup.h>
#if !defined(wxUSE_SECRETSTORE) || !wxUSE_SECRETSTORE
#error wxSecretStore support is disabled
#endif
int main() { return 0; }
" PERASTAGE_WX_SECRETSTORE_COMPILES)
    set(CMAKE_REQUIRED_INCLUDES "${_previous_required_includes}")
    set(${out_var} ${PERASTAGE_WX_SECRETSTORE_COMPILES} PARENT_SCOPE)
endfunction()

perastage_probe_wx_secretstore(PERASTAGE_WX_SECRETSTORE_ENABLED)
message(STATUS "Perastage secure credential store requirement: ${PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE}")
message(STATUS "Perastage target platform: ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "wxUSE_SECRETSTORE enabled: ${PERASTAGE_WX_SECRETSTORE_ENABLED}")
if(PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE AND NOT PERASTAGE_WX_SECRETSTORE_ENABLED)
    message(FATAL_ERROR
        "wxSecretStore support is missing from the configured wxWidgets build. "
        "Official Perastage builds require native credential-store support. "
        "When using vcpkg, install wxwidgets[secretstore] via the repository vcpkg manifest. "
        "On Linux, wxWidgets also needs libsecret development support (for example libsecret-1-dev) when it is built."
    )
endif()
