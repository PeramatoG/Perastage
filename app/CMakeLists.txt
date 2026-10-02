target_sources(${PROJECT_NAME} PRIVATE
    active_project_command_context.cpp
    active_project_command_context.h
    local_live_controller.cpp
    local_live_controller.h
    osc_controller.cpp
    osc_controller.h
    perastage_app.cpp
    perastage_app.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
