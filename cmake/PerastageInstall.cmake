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
install(FILES "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}"
        DESTINATION "${PERASTAGE_INSTALL_ASSET_DEST}/library/fixtures")
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
else()
    if(CMAKE_BUILD_TYPE)
        set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install/${CMAKE_BUILD_TYPE}")
    else()
        set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install")
    endif()
    set(PERASTAGE_STAGE_CONFIG)
endif()

add_custom_target(perastage_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PERASTAGE_STAGE_PREFIX}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${PERASTAGE_STAGE_PREFIX}" ${PERASTAGE_STAGE_CONFIG}
    DEPENDS ${PROJECT_NAME}
)
