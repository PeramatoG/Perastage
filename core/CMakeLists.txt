target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/autopatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configservices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/credentialstore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtfdictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtfnet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/guiconfigservices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutCollection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layoutviewpresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/markdown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/patchmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdftext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/PageSetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/Viewer2DPrintSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/projectutils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/riderimporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scenedatamanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simplecrypt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/trussdictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/trussloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/uuidutils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts
    ${CMAKE_CURRENT_SOURCE_DIR}/print
)
