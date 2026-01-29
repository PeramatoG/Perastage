cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

project(Perastage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NARM macOS
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()

include(CTest)

# wxWidgets chooses different binary sets based on wxWidgets_USE_DEBUG.
# - Single-config: we can select debug libs based on CMAKE_BUILD_TYPE.
# - Multi-config (Visual Studio): do not force wxWidgets_USE_DEBUG unless explicitly requested.
option(PERASTAGE_FORCE_WXDEBUG "Force linking wxWidgets debug libs" OFF)
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

if(CMAKE_CONFIGURATION_TYPES)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
endif()

# Find required packages
find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html richtext)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(GLEW REQUIRED)
find_package(nanovg CONFIG REQUIRED)
find_package(podofo CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Gather source files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/main.cpp
    # MainWindow modules split across gui/mainwindow_*.cpp (layout, menu, io, print).
    ${CMAKE_SOURCE_DIR}/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/models/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/core/layouts/*.cpp
    ${CMAKE_SOURCE_DIR}/core/print/*.cpp
    ${CMAKE_SOURCE_DIR}/mvr/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer3d/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer2d/*.cpp
)

# Explicitly ensure markdown conversion source is included
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/core/markdown.cpp)
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/core/uuidutils.cpp)

# On Windows, include application icon resources
if(WIN32)
    list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/resources/Perastage.rc)
endif()

# Optional: gather headers for IDEs
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/gui/*.h
    ${CMAKE_SOURCE_DIR}/models/*.h
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/core/layouts/*.h
    ${CMAKE_SOURCE_DIR}/core/print/*.h
    ${CMAKE_SOURCE_DIR}/mvr/*.h
    ${CMAKE_SOURCE_DIR}/viewer3d/*.h
    ${CMAKE_SOURCE_DIR}/viewer2d/*.h
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

# Mark as GUI application (no console)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

# Include project directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/mvr
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/viewer3d
    ${CMAKE_SOURCE_DIR}/viewer2d
    ${CMAKE_SOURCE_DIR}/resources
    ${NANOVG_INCLUDE_DIR}
)

# Link required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${wxWidgets_LIBRARIES}
    tinyxml2::tinyxml2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    CURL::libcurl
    nanovg::nanovg
    podofo::podofo
    ZLIB::ZLIB
)



if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources DESTINATION .)
if(EXISTS "${CMAKE_SOURCE_DIR}/library")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/library DESTINATION .)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/licenses")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/licenses DESTINATION .)
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE.txt
    ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    ${CMAKE_SOURCE_DIR}/help.md
    DESTINATION .
    OPTIONAL
)

if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    set(PERASTAGE_VCPKG_BIN_DIRS "")
    set(PERASTAGE_VCPKG_DEBUG_BIN_DIRS "")
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_perastage_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        if(EXISTS "${_perastage_vcpkg_bin}")
            list(APPEND PERASTAGE_VCPKG_BIN_DIRS "${_perastage_vcpkg_bin}")
        endif()
        set(_perastage_vcpkg_debug_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
        if(EXISTS "${_perastage_vcpkg_debug_bin}")
            list(APPEND PERASTAGE_VCPKG_DEBUG_BIN_DIRS "${_perastage_vcpkg_debug_bin}")
        endif()
    endif()

    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_prefix}/bin")
            list(APPEND PERASTAGE_VCPKG_BIN_DIRS "${_prefix}/bin")
        endif()
        if(EXISTS "${_prefix}/debug/bin")
            list(APPEND PERASTAGE_VCPKG_DEBUG_BIN_DIRS "${_prefix}/debug/bin")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_BIN_DIRS)
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_DEBUG_BIN_DIRS)

    if(CMAKE_CONFIGURATION_TYPES)
        install(TARGETS ${PROJECT_NAME}
            RUNTIME_DEPENDENCY_SET perastage_runtime_deps_release
            CONFIGURATIONS Release RelWithDebInfo MinSizeRel
        )
        install(RUNTIME_DEPENDENCY_SET perastage_runtime_deps_release
            DESTINATION .
            CONFIGURATIONS Release RelWithDebInfo MinSizeRel
            DIRECTORIES ${PERASTAGE_VCPKG_BIN_DIRS}
            PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[Ww]indows[\\/].*"
                ".*[Ss]ystem32[\\/].*"
                ".*ud_.*\\.dll$"
                ".*d_.*\\.dll$"
                ".*d\\.dll$"
        )
    elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        install(TARGETS ${PROJECT_NAME}
            RUNTIME_DEPENDENCY_SET perastage_runtime_deps_release
        )
        install(RUNTIME_DEPENDENCY_SET perastage_runtime_deps_release
            DESTINATION .
            DIRECTORIES ${PERASTAGE_VCPKG_BIN_DIRS}
            PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[Ww]indows[\\/].*"
                ".*[Ss]ystem32[\\/].*"
                ".*ud_.*\\.dll$"
                ".*d_.*\\.dll$"
                ".*d\\.dll$"
        )
    endif()

    # wxWidgets libraries may not appear in TARGET_RUNTIME_DLLS when discovered
    # via wxWidgets_USE_FILE, so derive DLL locations from linked libraries too.
    set(PERASTAGE_WX_SEARCH_DIRS "")
    foreach(wx_dir IN LISTS wxWidgets_LIBRARY_DIRS)
        if(wx_dir AND IS_DIRECTORY "${wx_dir}")
            list(APPEND PERASTAGE_WX_SEARCH_DIRS "${wx_dir}")
        endif()
    endforeach()
    foreach(wx_lib IN LISTS wxWidgets_LIBRARIES)
        if(EXISTS "${wx_lib}")
            get_filename_component(wx_lib_dir "${wx_lib}" DIRECTORY)
            if(IS_DIRECTORY "${wx_lib_dir}")
                list(APPEND PERASTAGE_WX_SEARCH_DIRS "${wx_lib_dir}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_WX_SEARCH_DIRS)

    set(PERASTAGE_WX_DLL_PATTERNS "")
    foreach(wx_dir IN LISTS PERASTAGE_WX_SEARCH_DIRS)
        list(APPEND PERASTAGE_WX_DLL_PATTERNS
            "${wx_dir}/wxmsw*.dll"
            "${wx_dir}/wxbase*.dll"
            "${wx_dir}/nwxmsw*.dll"
            "${wx_dir}/nwxbase*.dll")
        get_filename_component(wx_bin_dir "${wx_dir}/../bin" ABSOLUTE)
        list(APPEND PERASTAGE_WX_DLL_PATTERNS
            "${wx_bin_dir}/wxmsw*.dll"
            "${wx_bin_dir}/wxbase*.dll"
            "${wx_bin_dir}/nwxmsw*.dll"
            "${wx_bin_dir}/nwxbase*.dll")
    endforeach()
    list(REMOVE_ITEM PERASTAGE_WX_DLL_PATTERNS "")
    list(REMOVE_DUPLICATES PERASTAGE_WX_DLL_PATTERNS)

    set(PERASTAGE_WX_RUNTIME_DLLS "")
    foreach(wx_pattern IN LISTS PERASTAGE_WX_DLL_PATTERNS)
        file(GLOB PERASTAGE_WX_GLOB CONFIGURE_DEPENDS "${wx_pattern}")
        list(APPEND PERASTAGE_WX_RUNTIME_DLLS ${PERASTAGE_WX_GLOB})
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_WX_RUNTIME_DLLS)

    if(PERASTAGE_WX_RUNTIME_DLLS)
        set(PERASTAGE_WX_DEBUG_DLLS ${PERASTAGE_WX_RUNTIME_DLLS})
        set(PERASTAGE_WX_RELEASE_DLLS ${PERASTAGE_WX_RUNTIME_DLLS})
        list(FILTER PERASTAGE_WX_DEBUG_DLLS INCLUDE REGEX ".*(d_vc_|ud_vc_).*\\.dll$")
        list(FILTER PERASTAGE_WX_RELEASE_DLLS EXCLUDE REGEX ".*(d_vc_|ud_vc_).*\\.dll$")
        list(FILTER PERASTAGE_WX_RELEASE_DLLS EXCLUDE REGEX ".*ud_.*\\.dll$")
        list(FILTER PERASTAGE_WX_RELEASE_DLLS EXCLUDE REGEX ".*d_.*\\.dll$")
        list(FILTER PERASTAGE_WX_RELEASE_DLLS EXCLUDE REGEX ".*d\\.dll$")

        if(CMAKE_CONFIGURATION_TYPES)
            install(FILES ${PERASTAGE_WX_DEBUG_DLLS} DESTINATION . CONFIGURATIONS Debug)
            install(FILES ${PERASTAGE_WX_RELEASE_DLLS} DESTINATION . CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
        elseif(wxWidgets_USE_DEBUG)
            install(FILES ${PERASTAGE_WX_DEBUG_DLLS} DESTINATION .)
        else()
            install(FILES ${PERASTAGE_WX_RELEASE_DLLS} DESTINATION .)
        endif()
    endif()

endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install/$<CONFIG>")
    set(PERASTAGE_STAGE_CONFIG --config $<CONFIG>)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/install/$<CONFIG>/symbols")
else()
    set(PERASTAGE_STAGE_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(PERASTAGE_STAGE_CONFIG)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_INSTALL_PREFIX}/symbols")
endif()

add_custom_target(perastage_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PERASTAGE_STAGE_PREFIX}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${PERASTAGE_STAGE_PREFIX}" ${PERASTAGE_STAGE_CONFIG}
    DEPENDS ${PROJECT_NAME}
)

if(WIN32 AND MSVC)
    add_custom_target(perastage_symbols
        COMMAND ${CMAKE_COMMAND}
            -DPDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>
            -DPDB_DESTINATION="${PERASTAGE_SYMBOLS_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopySymbols.cmake"
        DEPENDS ${PROJECT_NAME}
    )
endif()

# Copy runtime resources next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/help.md $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/licenses $<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses
)

set(LIBRARY_SUBDIRS fixtures trusses misc "scene objects" projects)
foreach(subdir IN LISTS LIBRARY_SUBDIRS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/library/${subdir}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/library/${subdir}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}"
        )
    endif()
endforeach()
