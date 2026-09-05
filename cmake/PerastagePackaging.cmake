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
