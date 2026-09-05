# Own gettext discovery, catalog builds, and translation maintenance targets.

set(PERASTAGE_TRANSLATION_LANGUAGES es zh_CN)
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
    elseif(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
        set(PERASTAGE_VCPKG_INSTALLED_ROOT "$ENV{VCPKG_ROOT}/installed")
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
            message(FATAL_ERROR "PERASTAGE_ENABLE_LOCALIZATION is ON, but vcpkg gettext msgfmt.exe was not found. msgfmt is a build-time tool only and is not a Perastage runtime dependency. Expected msgfmt.exe: ${PERASTAGE_EXPECTED_VCPKG_MSGFMT}. Active vcpkg installed root: ${PERASTAGE_VCPKG_INSTALLED_ROOT}. Active vcpkg triplet: ${PERASTAGE_VCPKG_GETTEXT_TRIPLET}. For local Windows builds, set VCPKG_ROOT to the selected classic vcpkg checkout, install gettext[tools] for x64-windows, and keep VCPKG_MANIFEST_MODE=OFF; CI manifest builds must pass the same explicit installed root to CMake as VCPKG_INSTALLED_DIR. Then clear the affected CMake cache and reconfigure. To build an English-only development copy, explicitly configure with -DPERASTAGE_ENABLE_LOCALIZATION=OFF.")
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
