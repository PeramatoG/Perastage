cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

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

project(Perastage VERSION ${PERASTAGE_PROJECT_VERSION} LANGUAGES CXX)

set(PERASTAGE_APP_VERSION "${PROJECT_VERSION}")
set(PERASTAGE_APP_VERSION_DISPLAY "${PROJECT_VERSION}")
option(PERASTAGE_ASSOCIATE_PSTG "Register .pstg file association during packaging/installation where supported" ON)
option(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS "Enable MVR-xchange mDNS discovery using the vcpkg mdns package" ON)
option(PERASTAGE_ENABLE_LOCALIZATION "Enable gettext-based UI localization catalogs" ON)

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

add_executable(${PROJECT_NAME} main.cpp
    "${PERASTAGE_GENERATED_BUILD_INFO_CPP}"
    models/fixture.cpp
    models/continuous_placement_scene.cpp
    models/layer.cpp
    models/mvrscene.cpp
    models/sceneobject.cpp
    models/truss.cpp
    mvr/gdtf_catalog_matcher.cpp
    mvr/mvrimporter.cpp
    mvr/mvr_merge_analyzer.cpp
    mvr/mvr_merge_applier.cpp
    mvr/mvr_merge_resource_rewriter.cpp
    mvr/mvrscenemerger.cpp
    mvr/mvrexporter.cpp
    mvr/primitive_model_resources.cpp
    mvr/xchange/mvr_xchange_commit.cpp
    mvr/xchange/mvr_xchange_dns_names.cpp
    mvr/xchange/mvr_xchange_mdns_discovery.cpp
    mvr/xchange/mvr_xchange_mdns_service.cpp
    mvr/xchange/mvr_xchange_network_interfaces.cpp
    mvr/xchange/mvr_xchange_remote_station.cpp
    mvr/xchange/mvr_xchange_station_registry.cpp
    mvr/xchange/mvr_xchange_tcp_client.cpp
    mvr/xchange/mvr_xchange_message.cpp
    mvr/xchange/mvr_xchange_packet.cpp
    mvr/xchange/mvr_xchange_service.cpp
    mvr/xchange/mvr_xchange_settings.cpp
    mvr/xchange/mvr_xchange_tcp_server.cpp
)

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

set(PERASTAGE_TRANSLATION_LANGUAGES es)
set(PERASTAGE_LOCALE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources/locale")
set(PERASTAGE_POT_FILE "${PERASTAGE_LOCALE_SOURCE_DIR}/perastage.pot")
set(PERASTAGE_GENERATED_LOCALE_DIR "${CMAKE_BINARY_DIR}/generated/locale")
set(PERASTAGE_TRANSLATION_OUTPUTS "")
set(PERASTAGE_MSGFMT_HINTS "")
set(PERASTAGE_GETTEXT_PROVIDER "")
set(PERASTAGE_VCPKG_INSTALLED_ROOT "")
set(PERASTAGE_VCPKG_GETTEXT_TRIPLET "")
set(PERASTAGE_EXPECTED_VCPKG_MSGFMT "")
set(PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR "")
if(WIN32)
    if(DEFINED VCPKG_INSTALLED_DIR AND NOT "${VCPKG_INSTALLED_DIR}" STREQUAL "")
        set(PERASTAGE_VCPKG_INSTALLED_ROOT "${VCPKG_INSTALLED_DIR}")
    elseif(DEFINED CMAKE_TOOLCHAIN_FILE AND
           CMAKE_TOOLCHAIN_FILE MATCHES "[/\\]scripts[/\\]buildsystems[/\\]vcpkg\\.cmake$")
        get_filename_component(_perastage_vcpkg_buildsystems_dir "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
        get_filename_component(_perastage_vcpkg_scripts_dir "${_perastage_vcpkg_buildsystems_dir}" DIRECTORY)
        get_filename_component(_perastage_vcpkg_root "${_perastage_vcpkg_scripts_dir}" DIRECTORY)
        set(PERASTAGE_VCPKG_INSTALLED_ROOT "${_perastage_vcpkg_root}/installed")
    endif()

    if(DEFINED VCPKG_TARGET_TRIPLET AND NOT "${VCPKG_TARGET_TRIPLET}" STREQUAL "")
        set(PERASTAGE_VCPKG_GETTEXT_TRIPLET "${VCPKG_TARGET_TRIPLET}")
    else()
        set(PERASTAGE_VCPKG_GETTEXT_TRIPLET "x64-windows")
    endif()

    if(NOT PERASTAGE_VCPKG_INSTALLED_ROOT STREQUAL "")
        set(PERASTAGE_EXPECTED_VCPKG_MSGFMT
            "${PERASTAGE_VCPKG_INSTALLED_ROOT}/${PERASTAGE_VCPKG_GETTEXT_TRIPLET}/tools/gettext/bin/msgfmt.exe")
        get_filename_component(PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR
            "${PERASTAGE_EXPECTED_VCPKG_MSGFMT}" DIRECTORY)
        find_program(PERASTAGE_MSGFMT_EXECUTABLE
            NAMES msgfmt.exe
            HINTS "${PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        find_program(PERASTAGE_XGETTEXT_EXECUTABLE
            NAMES xgettext.exe
            HINTS "${PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        find_program(PERASTAGE_MSGMERGE_EXECUTABLE
            NAMES msgmerge.exe
            HINTS "${PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        find_program(PERASTAGE_MSGATTRIB_EXECUTABLE
            NAMES msgattrib.exe
            HINTS "${PERASTAGE_EXPECTED_VCPKG_GETTEXT_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        if(PERASTAGE_MSGFMT_EXECUTABLE)
            set(PERASTAGE_GETTEXT_PROVIDER "vcpkg")
        endif()
    endif()
else()
    find_package(Gettext QUIET)
    find_program(PERASTAGE_MSGFMT_EXECUTABLE NAMES msgfmt)
    find_program(PERASTAGE_XGETTEXT_EXECUTABLE NAMES xgettext)
    find_program(PERASTAGE_MSGMERGE_EXECUTABLE NAMES msgmerge)
    find_program(PERASTAGE_MSGATTRIB_EXECUTABLE NAMES msgattrib)
    if(GETTEXT_MSGFMT_EXECUTABLE AND NOT PERASTAGE_MSGFMT_EXECUTABLE)
        set(PERASTAGE_MSGFMT_EXECUTABLE "${GETTEXT_MSGFMT_EXECUTABLE}")
    endif()
    if(PERASTAGE_MSGFMT_EXECUTABLE)
        set(PERASTAGE_GETTEXT_PROVIDER "system")
    endif()
endif()
if(PERASTAGE_MSGFMT_EXECUTABLE)
    get_filename_component(PERASTAGE_MSGFMT_DIRECTORY "${PERASTAGE_MSGFMT_EXECUTABLE}" DIRECTORY)
endif()
if(PERASTAGE_ENABLE_LOCALIZATION)
    if(NOT PERASTAGE_MSGFMT_EXECUTABLE)
        if(WIN32)
            message(FATAL_ERROR "PERASTAGE_ENABLE_LOCALIZATION is ON, but vcpkg gettext msgfmt.exe was not found. msgfmt is a build-time tool only and is not a Perastage runtime dependency. Expected msgfmt.exe: ${PERASTAGE_EXPECTED_VCPKG_MSGFMT}. Active vcpkg installed root: ${PERASTAGE_VCPKG_INSTALLED_ROOT}. Active vcpkg triplet: ${PERASTAGE_VCPKG_GETTEXT_TRIPLET}. For the default setup, run: C:\\vcpkg\\vcpkg.exe install \"gettext[tools]:x64-windows\". If you use another vcpkg root, run the equivalent command with that configured vcpkg.exe. Then clear the affected CMake cache and reconfigure. To build an English-only development copy, explicitly configure with -DPERASTAGE_ENABLE_LOCALIZATION=OFF.")
        else()
            message(FATAL_ERROR "PERASTAGE_ENABLE_LOCALIZATION is ON, but GNU gettext msgfmt was not found. msgfmt is a build-time tool only and is not a Perastage runtime dependency. Install gettext/msgfmt and reconfigure: on macOS use Homebrew gettext; on Linux install the gettext package. To build an English-only development copy, explicitly configure with -DPERASTAGE_ENABLE_LOCALIZATION=OFF.")
        endif()
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_EXECUTABLE)
        message(WARNING "Python3 was not found; gettext catalog compilation remains available, but perastage_update_pot, perastage_update_po, and perastage_check_translations will report that Python3 is required.")
    endif()
    message(STATUS "Perastage gettext provider: ${PERASTAGE_GETTEXT_PROVIDER}")
    message(STATUS "Perastage gettext msgfmt: ${PERASTAGE_MSGFMT_EXECUTABLE}")
    message(STATUS "Perastage gettext bin dir: ${PERASTAGE_MSGFMT_DIRECTORY}")
    message(STATUS "Perastage gettext xgettext: ${PERASTAGE_XGETTEXT_EXECUTABLE}")
    message(STATUS "Perastage gettext msgmerge: ${PERASTAGE_MSGMERGE_EXECUTABLE}")
    message(STATUS "Perastage gettext msgattrib: ${PERASTAGE_MSGATTRIB_EXECUTABLE}")
    if(WIN32)
        set(PERASTAGE_MSGFMT_ENV_PATH "PATH=${PERASTAGE_MSGFMT_DIRECTORY}$<SEMICOLON>$ENV{PATH}")
        set(PERASTAGE_GETTEXT_ENV_PATH "PATH=${PERASTAGE_MSGFMT_DIRECTORY}$<SEMICOLON>$ENV{PATH}")
    else()
        set(PERASTAGE_MSGFMT_ENV_PATH "PATH=${PERASTAGE_MSGFMT_DIRECTORY}:$ENV{PATH}")
        set(PERASTAGE_GETTEXT_ENV_PATH "PATH=${PERASTAGE_MSGFMT_DIRECTORY}:$ENV{PATH}")
    endif()
    foreach(PERASTAGE_TRANSLATION_LANGUAGE IN LISTS PERASTAGE_TRANSLATION_LANGUAGES)
        set(PERASTAGE_PO_FILE "${PERASTAGE_LOCALE_SOURCE_DIR}/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES/perastage.po")
        set(PERASTAGE_MO_FILE "${PERASTAGE_GENERATED_LOCALE_DIR}/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES/perastage.mo")
        add_custom_command(
            OUTPUT "${PERASTAGE_MO_FILE}"
            COMMAND "${CMAKE_COMMAND}"
                    "-DMSGFMT_EXECUTABLE=${PERASTAGE_MSGFMT_EXECUTABLE}"
                    "-DGETTEXT_BIN_DIR=${PERASTAGE_MSGFMT_DIRECTORY}"
                    "-DINPUT_PO=${PERASTAGE_PO_FILE}"
                    "-DOUTPUT_MO=${PERASTAGE_MO_FILE}"
                    -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCompileGettextCatalog.cmake"
            DEPENDS
                    "${PERASTAGE_PO_FILE}"
                    "${CMAKE_SOURCE_DIR}/cmake/PerastageCompileGettextCatalog.cmake"
            COMMENT "Generating ${PERASTAGE_TRANSLATION_LANGUAGE} gettext catalog"
            VERBATIM
        )
        list(APPEND PERASTAGE_TRANSLATION_OUTPUTS "${PERASTAGE_MO_FILE}")
    endforeach()
    add_custom_target(perastage_translations DEPENDS ${PERASTAGE_TRANSLATION_OUTPUTS})
    add_dependencies(${PROJECT_NAME} perastage_translations)
    if(Python3_EXECUTABLE)
        add_custom_target(perastage_update_pot
            COMMAND ${CMAKE_COMMAND} -E env "${PERASTAGE_GETTEXT_ENV_PATH}" "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/localization_catalog.py" update-pot --xgettext "${PERASTAGE_XGETTEXT_EXECUTABLE}" --msgmerge "${PERASTAGE_MSGMERGE_EXECUTABLE}" --msgfmt "${PERASTAGE_MSGFMT_EXECUTABLE}" --msgattrib "${PERASTAGE_MSGATTRIB_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Updating gettext POT template"
            VERBATIM
        )
        add_custom_target(perastage_update_po
            COMMAND ${CMAKE_COMMAND} -E env "${PERASTAGE_GETTEXT_ENV_PATH}" "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/localization_catalog.py" update-po --xgettext "${PERASTAGE_XGETTEXT_EXECUTABLE}" --msgmerge "${PERASTAGE_MSGMERGE_EXECUTABLE}" --msgfmt "${PERASTAGE_MSGFMT_EXECUTABLE}" --msgattrib "${PERASTAGE_MSGATTRIB_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Merging gettext POT template into PO catalogs"
            VERBATIM
        )
        add_custom_target(perastage_check_translations
            COMMAND ${CMAKE_COMMAND} -E env "${PERASTAGE_GETTEXT_ENV_PATH}" "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/localization_catalog.py" check-po --xgettext "${PERASTAGE_XGETTEXT_EXECUTABLE}" --msgmerge "${PERASTAGE_MSGMERGE_EXECUTABLE}" --msgfmt "${PERASTAGE_MSGFMT_EXECUTABLE}" --msgattrib "${PERASTAGE_MSGATTRIB_EXECUTABLE}"
            COMMAND ${CMAKE_COMMAND} -E env "${PERASTAGE_GETTEXT_ENV_PATH}" "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/localization_catalog.py" audit
            COMMAND ${CMAKE_COMMAND} -E env "${PERASTAGE_GETTEXT_ENV_PATH}" "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/localization_catalog.py" self-test
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Checking gettext catalogs and high-confidence UI string marking"
            VERBATIM
        )
    else()
        add_custom_target(perastage_update_pot
            COMMAND ${CMAKE_COMMAND} -E echo "Python3 is required to update Perastage gettext templates."
            COMMAND ${CMAKE_COMMAND} -E false
        )
        add_custom_target(perastage_update_po
            COMMAND ${CMAKE_COMMAND} -E echo "Python3 is required to merge Perastage gettext catalogs."
            COMMAND ${CMAKE_COMMAND} -E false
        )
        add_custom_target(perastage_check_translations
            COMMAND ${CMAKE_COMMAND} -E echo "Python3 is required to run Perastage translation checks."
            COMMAND ${CMAKE_COMMAND} -E false
        )
    endif()
else()
    message(STATUS "Perastage UI localization is disabled; building English-only UI.")
    add_custom_target(perastage_translations)
endif()
add_custom_target(perastage_update_catalogs DEPENDS perastage_translations)

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
add_subdirectory(viewer2d)
add_subdirectory(viewer3d)
add_subdirectory(viewer_common)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resources/Perastage.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 iphlpapi)
endif()
if(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PERASTAGE_MVR_XCHANGE_ENABLE_MDNS=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE mdns::mdns)
endif()

# Mark as GUI application (no console)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

if(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION .
    )
else()
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
endif()

if(UNIX AND NOT APPLE)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/linux/perastage.desktop.in"
        "${CMAKE_BINARY_DIR}/perastage.desktop"
        @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/perastage.desktop"
        DESTINATION share/applications
    )
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/perastage-mime.xml"
        DESTINATION share/mime/packages
    )
    install(FILES "${CMAKE_SOURCE_DIR}/resources/Perastage_logo_1024.png"
        DESTINATION share/icons/hicolor/1024x1024/apps
        RENAME perastage.png
    )
    install(CODE [[
        execute_process(
            COMMAND update-mime-database "${CMAKE_INSTALL_PREFIX}/share/mime"
            RESULT_VARIABLE perastage_mime_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        execute_process(
            COMMAND update-desktop-database "${CMAKE_INSTALL_PREFIX}/share/applications"
            RESULT_VARIABLE perastage_desktop_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
    ]])
endif()

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

if(APPLE)
    # Keep runtime assets inside the app bundle so packaged macOS builds resolve resources correctly.
    set(PERASTAGE_INSTALL_ASSET_DEST "${PROJECT_NAME}.app/Contents/Resources")
    set(PERASTAGE_INSTALL_RESOURCE_DEST "${PROJECT_NAME}.app/Contents/Resources")
else()
    # Keep runtime assets next to the executable on non-macOS platforms.
    set(PERASTAGE_INSTALL_ASSET_DEST ".")
    set(PERASTAGE_INSTALL_RESOURCE_DEST "${PERASTAGE_INSTALL_ASSET_DEST}")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${PERASTAGE_INSTALL_RESOURCE_DEST})
if(PERASTAGE_ENABLE_LOCALIZATION)
    foreach(PERASTAGE_TRANSLATION_LANGUAGE IN LISTS PERASTAGE_TRANSLATION_LANGUAGES)
        if(APPLE)
            set(PERASTAGE_INSTALL_LOCALE_DEST "${PERASTAGE_INSTALL_RESOURCE_DEST}/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        else()
            set(PERASTAGE_INSTALL_LOCALE_DEST "${PERASTAGE_INSTALL_RESOURCE_DEST}/resources/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        endif()
        install(FILES "${PERASTAGE_GENERATED_LOCALE_DIR}/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES/perastage.mo" DESTINATION "${PERASTAGE_INSTALL_LOCALE_DEST}")
    endforeach()
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/library")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/library DESTINATION ${PERASTAGE_INSTALL_ASSET_DEST})
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/licenses")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/licenses DESTINATION ${PERASTAGE_INSTALL_ASSET_DEST})
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE.txt
    ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    ${CMAKE_SOURCE_DIR}/help.md
    DESTINATION ${PERASTAGE_INSTALL_ASSET_DEST}
    OPTIONAL
)

if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    # Collect vcpkg release and debug bin directories.
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
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_BIN_DIRS)
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_DEBUG_BIN_DIRS)

    # Stage all DLLs from vcpkg/bin directly via glob.
    # This is simpler and more reliable than file(GET_RUNTIME_DEPENDENCIES),
    # which struggles with virtual Windows API-set DLLs that have no physical
    # file on disk. vcpkg/bin contains only the third-party runtime DLLs we
    # need — no Windows system DLLs — so a plain glob is safe here.
    set(PERASTAGE_VCPKG_RELEASE_DLLS "")
    set(PERASTAGE_VCPKG_DEBUG_DLLS "")
    foreach(_vcpkg_bin IN LISTS PERASTAGE_VCPKG_BIN_DIRS)
        file(GLOB _vcpkg_bin_dlls "${_vcpkg_bin}/*.dll")
        list(APPEND PERASTAGE_VCPKG_RELEASE_DLLS ${_vcpkg_bin_dlls})
    endforeach()
    foreach(_vcpkg_debug_bin IN LISTS PERASTAGE_VCPKG_DEBUG_BIN_DIRS)
        file(GLOB _vcpkg_debug_bin_dlls "${_vcpkg_debug_bin}/*.dll")
        list(APPEND PERASTAGE_VCPKG_DEBUG_DLLS ${_vcpkg_debug_bin_dlls})
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_RELEASE_DLLS)
    list(REMOVE_DUPLICATES PERASTAGE_VCPKG_DEBUG_DLLS)

    if(PERASTAGE_VCPKG_RELEASE_DLLS)
        if(CMAKE_CONFIGURATION_TYPES)
            install(FILES ${PERASTAGE_VCPKG_RELEASE_DLLS} DESTINATION . CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
            install(FILES ${PERASTAGE_VCPKG_DEBUG_DLLS}   DESTINATION . CONFIGURATIONS Debug)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            install(FILES ${PERASTAGE_VCPKG_DEBUG_DLLS} DESTINATION .)
        else()
            install(FILES ${PERASTAGE_VCPKG_RELEASE_DLLS} DESTINATION .)
        endif()
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
        file(GLOB PERASTAGE_WX_GLOB "${wx_pattern}")
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
    if(CMAKE_BUILD_TYPE)
        set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install/${CMAKE_BUILD_TYPE}")
        set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/install/${CMAKE_BUILD_TYPE}/symbols")
    else()
        set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install")
        set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/install/symbols")
    endif()
    set(PERASTAGE_STAGE_CONFIG)
endif()

add_custom_target(perastage_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PERASTAGE_STAGE_PREFIX}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${PERASTAGE_STAGE_PREFIX}" ${PERASTAGE_STAGE_CONFIG}
    DEPENDS ${PROJECT_NAME}
)

if(WIN32 AND MSVC)
    add_custom_target(perastage_symbols
        COMMAND ${CMAKE_COMMAND}
            "-DPDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "-DPDB_DESTINATION=${PERASTAGE_SYMBOLS_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopySymbols.cmake"
        DEPENDS ${PROJECT_NAME}
    )
endif()

if(APPLE)
    set(PERASTAGE_RUNTIME_ASSET_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
else()
    set(PERASTAGE_RUNTIME_ASSET_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()

# Copy runtime resources to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERASTAGE_RUNTIME_ASSET_DIR}
)

if(APPLE)
    # Copy resource files directly into Contents/Resources for macOS bundle consistency.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources
                ${PERASTAGE_RUNTIME_ASSET_DIR}
    )
else()
    # Copy resource files into a dedicated resources/ directory on non-macOS platforms.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources
                ${PERASTAGE_RUNTIME_ASSET_DIR}/resources
    )
endif()

if(PERASTAGE_ENABLE_LOCALIZATION)
    foreach(PERASTAGE_TRANSLATION_LANGUAGE IN LISTS PERASTAGE_TRANSLATION_LANGUAGES)
        if(APPLE)
            set(PERASTAGE_RUNTIME_LOCALE_DIR "${PERASTAGE_RUNTIME_ASSET_DIR}/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        else()
            set(PERASTAGE_RUNTIME_LOCALE_DIR "${PERASTAGE_RUNTIME_ASSET_DIR}/resources/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        endif()
        set(PERASTAGE_RUNTIME_MO "${PERASTAGE_GENERATED_LOCALE_DIR}/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES/perastage.mo")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_MO=${PERASTAGE_RUNTIME_MO}"
                    "-DDESTINATION_MO=${PERASTAGE_RUNTIME_LOCALE_DIR}/perastage.mo"
                    -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopyRuntimeCatalog.cmake"
            COMMAND "${CMAKE_COMMAND}"
                    "-DEXPECTED_FILE=${PERASTAGE_RUNTIME_LOCALE_DIR}/perastage.mo"
                    -P "${CMAKE_SOURCE_DIR}/cmake/PerastageVerifyFileExists.cmake"
            VERBATIM
        )
    endforeach()
endif()

# Copy help documentation to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/help.md ${PERASTAGE_RUNTIME_ASSET_DIR}
)

# Copy license files to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE.txt ${PERASTAGE_RUNTIME_ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md ${PERASTAGE_RUNTIME_ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/licenses ${PERASTAGE_RUNTIME_ASSET_DIR}/licenses
)

set(LIBRARY_SUBDIRS fixtures trusses misc scene_objects projects default_layouts hoists)
foreach(subdir IN LISTS LIBRARY_SUBDIRS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PERASTAGE_RUNTIME_ASSET_DIR}/library/${subdir}"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/library/${subdir}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/library/${subdir}"
                    "${PERASTAGE_RUNTIME_ASSET_DIR}/library/${subdir}"
        )
    endif()
endforeach()

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
