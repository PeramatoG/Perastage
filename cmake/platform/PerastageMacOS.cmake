# Configure the macOS application bundle and its document icon resources.
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
