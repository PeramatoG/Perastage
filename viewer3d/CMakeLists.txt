target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/culling/bounds_cache_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/culling/visibilitysystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtfloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/labels/label_render_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loader3ds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaderglb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/meshprimitives.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/picking/selectionsystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/gl_primitive_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/opaque_fixture_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/opaque_object_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/opaque_pass_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/opaque_truss_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/render_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/scenerenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resource_sync_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer3dcamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer3dcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewer3dpanel.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${CMAKE_CURRENT_SOURCE_DIR}/culling
    ${CMAKE_CURRENT_SOURCE_DIR}/labels
    ${CMAKE_CURRENT_SOURCE_DIR}/picking
    ${CMAKE_CURRENT_SOURCE_DIR}/render
)
