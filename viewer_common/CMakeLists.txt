target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_canvas_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_context_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measure_overlay_style.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
