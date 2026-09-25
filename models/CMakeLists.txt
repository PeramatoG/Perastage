add_library(perastage_mvr_scene_model STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrscene.cpp
)
target_compile_features(perastage_mvr_scene_model PUBLIC cxx_std_20)
target_include_directories(perastage_mvr_scene_model PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/continuous_placement_scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/continuous_placement_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sceneobject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
