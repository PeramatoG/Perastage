target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas2d.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf/font_metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf/layout_pdf_exporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf/pdf_graphics_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf/pdf_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print_diagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbolcache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2dcommandrenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2doffscreenrenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2dpanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2dpdfexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2drenderpanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer2dstate.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf
)
