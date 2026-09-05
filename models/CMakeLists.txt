target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/continuous_placement_scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/continuous_placement_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrscene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sceneobject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
