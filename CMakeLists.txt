cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

set(PERASTAGE_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
if(NOT EXISTS "${PERASTAGE_VERSION_FILE}")
    message(FATAL_ERROR "Missing VERSION file at ${PERASTAGE_VERSION_FILE}.")
endif()

file(READ "${PERASTAGE_VERSION_FILE}" PERASTAGE_PROJECT_VERSION_RAW)
string(STRIP "${PERASTAGE_PROJECT_VERSION_RAW}" PERASTAGE_PROJECT_VERSION)
if(NOT PERASTAGE_PROJECT_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR
        "Invalid version '${PERASTAGE_PROJECT_VERSION}' in ${PERASTAGE_VERSION_FILE}. "
        "Expected MAJOR.MINOR.PATCH."
    )
endif()

project(
    Perastage
    VERSION ${PERASTAGE_PROJECT_VERSION}
    LANGUAGES C CXX
)

set(PERASTAGE_APP_VERSION "${PROJECT_VERSION}")
set(PERASTAGE_APP_VERSION_DISPLAY "${PROJECT_VERSION}")
option(PERASTAGE_ASSOCIATE_PSTG "Register .pstg file association during packaging/installation where supported" ON)
option(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS "Enable MVR-xchange mDNS discovery using the vcpkg mdns package" ON)
option(PERASTAGE_ENABLE_LOCALIZATION "Enable gettext-based UI localization catalogs" ON)
option(PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE "Require wxSecretStore support in the wxWidgets build" OFF)
option(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT
       "Use legacy worker-thread fallbacks for the macOS 15 hosted toolchain" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PERASTAGE_ENABLE_COMPILER_CACHE "Enable compiler cache launcher when available" ON)
if(PERASTAGE_ENABLE_COMPILER_CACHE)
    find_program(PERASTAGE_COMPILER_CACHE_PROGRAM NAMES sccache ccache)
    if(PERASTAGE_COMPILER_CACHE_PROGRAM)
        message(STATUS "Using compiler cache launcher: ${PERASTAGE_COMPILER_CACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${PERASTAGE_COMPILER_CACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${PERASTAGE_COMPILER_CACHE_PROGRAM}")
    endif()
endif()

option(PERASTAGE_FORCE_WXDEBUG "Force linking wxWidgets debug libs" OFF)

get_property(_perastage_build_testing_set CACHE BUILD_TESTING PROPERTY VALUE SET)
if(NOT _perastage_build_testing_set)
    if(CMAKE_CONFIGURATION_TYPES)
        set(BUILD_TESTING OFF CACHE BOOL "Build tests")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BUILD_TESTING ON CACHE BOOL "Build tests")
    else()
        set(BUILD_TESTING OFF CACHE BOOL "Build tests")
    endif()
elseif(BUILD_TESTING AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "BUILD_TESTING=ON was explicitly requested for a non-Debug build; packaging presets keep tests off by default.")
endif()
if(BUILD_TESTING)
    enable_testing()
endif()

include(cmake/PerastageDependencies.cmake)

set(PERASTAGE_DUMMY_GDTF_DESCRIPTION "${CMAKE_SOURCE_DIR}/cmake/fixtures/Dummy1ch.description.xml")
set(PERASTAGE_GENERATED_LIBRARY_DIR "${CMAKE_BINARY_DIR}/generated/library")
set(PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE "${PERASTAGE_GENERATED_LIBRARY_DIR}/fixtures/Dummy 1ch.gdtf")
add_custom_command(
    OUTPUT "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/cmake/PerastageGenerateGdtfArchive.py"
            "${PERASTAGE_DUMMY_GDTF_DESCRIPTION}"
            "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}"
    DEPENDS
            "${CMAKE_SOURCE_DIR}/cmake/PerastageGenerateGdtfArchive.py"
            "${PERASTAGE_DUMMY_GDTF_DESCRIPTION}"
    VERBATIM
)
add_custom_target(perastage_generated_dummy_gdtf ALL
    DEPENDS "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}"
)

# Reconfigure generated build identity when HEAD or its active branch ref moves.
execute_process(
    COMMAND git rev-parse --git-path HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PERASTAGE_GIT_HEAD_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git symbolic-ref -q HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PERASTAGE_GIT_HEAD_REF
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(PERASTAGE_GIT_HEAD_PATH)
    if(NOT IS_ABSOLUTE "${PERASTAGE_GIT_HEAD_PATH}")
        set(PERASTAGE_GIT_HEAD_PATH
            "${CMAKE_SOURCE_DIR}/${PERASTAGE_GIT_HEAD_PATH}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${PERASTAGE_GIT_HEAD_PATH}")
endif()
if(PERASTAGE_GIT_HEAD_REF)
    execute_process(
        COMMAND git rev-parse --git-path "${PERASTAGE_GIT_HEAD_REF}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE PERASTAGE_GIT_REF_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(PERASTAGE_GIT_REF_PATH)
        if(NOT IS_ABSOLUTE "${PERASTAGE_GIT_REF_PATH}")
            set(PERASTAGE_GIT_REF_PATH
                "${CMAKE_SOURCE_DIR}/${PERASTAGE_GIT_REF_PATH}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${PERASTAGE_GIT_REF_PATH}")
    endif()
endif()

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PERASTAGE_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT PERASTAGE_GIT_COMMIT)
    set(PERASTAGE_GIT_COMMIT "unknown")
endif()
string(TIMESTAMP PERASTAGE_BUILD_TIMESTAMP_UTC "%Y-%m-%d %H:%M:%S UTC" UTC)
if(CMAKE_CONFIGURATION_TYPES)
    set(PERASTAGE_BUILD_TYPE_TEXT "$<CONFIG>")
elseif(CMAKE_BUILD_TYPE)
    set(PERASTAGE_BUILD_TYPE_TEXT "${CMAKE_BUILD_TYPE}")
else()
    set(PERASTAGE_BUILD_TYPE_TEXT "Unknown")
endif()
set(PERASTAGE_TARGET_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(PERASTAGE_COMPILER_TEXT "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
set(PERASTAGE_GENERATED_BUILD_INFO_CPP "${CMAKE_BINARY_DIR}/generated/build_info.cpp")
configure_file(
    "${CMAKE_SOURCE_DIR}/core/build_info.cpp.in"
    "${PERASTAGE_GENERATED_BUILD_INFO_CPP}"
    @ONLY
)

add_library(perastage_uuid OBJECT)

add_executable(${PROJECT_NAME}
    main.cpp
    "${PERASTAGE_GENERATED_BUILD_INFO_CPP}"
)

if(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT=1)
endif()

if(APPLE)
    set(PERASTAGE_MACOS_INFO_PLIST "${CMAKE_BINARY_DIR}/PerastageInfo.plist")
    set(EXECUTABLE_NAME "${PROJECT_NAME}")
    set(PERASTAGE_VERSION "${PROJECT_VERSION}")

    set(PERASTAGE_MACOS_RESOURCES
        "${CMAKE_SOURCE_DIR}/resources/Perastage.icns"
        "${CMAKE_SOURCE_DIR}/resources/PerastageProject.icns"
        "${CMAKE_SOURCE_DIR}/resources/PerastageMVR.icns"
    )

    target_sources(${PROJECT_NAME} PRIVATE
        ${PERASTAGE_MACOS_RESOURCES}
    )

    set_source_files_properties(${PERASTAGE_MACOS_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/PerastageInfo.plist.in"
        "${PERASTAGE_MACOS_INFO_PLIST}"
        @ONLY
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${PERASTAGE_MACOS_INFO_PLIST}"
        RESOURCE "${PERASTAGE_MACOS_RESOURCES}"
    )
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API=1
    PERASTAGE_ENABLE_LOCALIZATION=$<BOOL:${PERASTAGE_ENABLE_LOCALIZATION}>
)

add_dependencies(${PROJECT_NAME} perastage_generated_dummy_gdtf)
include(cmake/PerastageLocalization.cmake)

set_source_files_properties("${PERASTAGE_GENERATED_BUILD_INFO_CPP}" PROPERTIES
    COMPILE_DEFINITIONS "PERASTAGE_BUILD_CONFIGURATION=\"$<CONFIG>\""
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
    target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:/DEBUG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )
endif()

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(models)
add_subdirectory(mvr)
add_subdirectory(viewer2d)
add_subdirectory(viewer3d)
add_subdirectory(viewer_common)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resources/Perastage.rc)
endif()

# Mark as GUI application (no console)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)


# Include project directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/gui/mainwindow/controllers
    ${CMAKE_SOURCE_DIR}/gui/mainwindow/ids
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/layouts
    ${CMAKE_SOURCE_DIR}/core/print
    ${CMAKE_SOURCE_DIR}/mvr
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/viewer3d
    ${CMAKE_SOURCE_DIR}/viewer3d/interfaces
    ${CMAKE_SOURCE_DIR}/viewer3d/resources
    ${CMAKE_SOURCE_DIR}/viewer3d/culling
    ${CMAKE_SOURCE_DIR}/viewer3d/labels
    ${CMAKE_SOURCE_DIR}/viewer3d/picking
    ${CMAKE_SOURCE_DIR}/viewer3d/render
    ${CMAKE_SOURCE_DIR}/viewer2d
    ${CMAKE_SOURCE_DIR}/viewer2d/pdf
    ${CMAKE_SOURCE_DIR}/resources
    ${_wx_includes}
    ${NANOVG_INCLUDE_DIR}
)

# Link required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    perastage_uuid
    tinyxml2::tinyxml2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    CURL::libcurl
    meshoptimizer::meshoptimizer
    nanovg::nanovg
    podofo::podofo
    ZLIB::ZLIB
    Backward::Backward
    ${_wx_libs}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Dbghelp)
endif()







if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(cmake/PerastageInstall.cmake)

if(CMAKE_CONFIGURATION_TYPES)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/symbols/windows")
else()
    if(CMAKE_BUILD_TYPE)
        set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/symbols/windows")
    else()
        set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/symbols/windows")
    endif()
endif()

if(WIN32 AND MSVC)
    add_custom_target(perastage_symbols
        COMMAND ${CMAKE_COMMAND}
            "-DPDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "-DPDB_DESTINATION=${PERASTAGE_SYMBOLS_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopySymbols.cmake"
        DEPENDS ${PROJECT_NAME}
    )
endif()

include(cmake/PerastageRuntimeStaging.cmake)

set(CPACK_PACKAGE_NAME "Perastage")
set(CPACK_PACKAGE_VENDOR "Perastage")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Perastage lighting plot editor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "Perastage")
    set(CPACK_NSIS_PACKAGE_NAME "Perastage")
    string(CONCAT CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "WriteRegStr HKCR '.mvr' '' 'Perastage.MVR'\\n"
        "WriteRegStr HKCR 'Perastage.MVR' '' 'MVR Scene'\\n"
        "WriteRegStr HKCR 'Perastage.MVR\\\\DefaultIcon' '' '$INSTDIR\\\\Perastage.exe,0'\\n"
        "WriteRegStr HKCR 'Perastage.MVR\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\Perastage.exe\\\" \\\"%1\\\"'\\n"
    )
    string(CONCAT CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "DeleteRegKey HKCR 'Perastage.MVR'\\n"
        "DeleteRegValue HKCR '.mvr' ''\\n"
    )
    if(PERASTAGE_ASSOCIATE_PSTG)
        string(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS
            "WriteRegStr HKCR '.pstg' '' 'Perastage.Project'\\n"
            "WriteRegStr HKCR 'Perastage.Project' '' 'Perastage Project'\\n"
            "WriteRegStr HKCR 'Perastage.Project\\\\DefaultIcon' '' '$INSTDIR\\\\Perastage.exe,0'\\n"
            "WriteRegStr HKCR 'Perastage.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\Perastage.exe\\\" \\\"%1\\\"'\\n"
        )
        string(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
            "DeleteRegKey HKCR 'Perastage.Project'\\n"
            "DeleteRegValue HKCR '.pstg' ''\\n"
        )
    endif()
endif()

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Perastage")
    set(CPACK_PACKAGE_FILE_NAME "Perastage-${PROJECT_VERSION}-macOS-arm64")
endif()

include(CPack)
