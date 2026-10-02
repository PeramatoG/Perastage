target_sources(${PROJECT_NAME} PRIVATE
    local_live_controller.cpp
    local_live_controller.h
    perastage_app.cpp
    perastage_app.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
