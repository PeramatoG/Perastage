cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

project(Perastage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(wxWidgets REQUIRED COMPONENTS core base aui gl)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(GLEW REQUIRED)
find_package(nanovg CONFIG REQUIRED)

# Gather source files
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/models/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/mvr/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer3d/*.cpp
)

# Optional: gather headers for IDEs
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/gui/*.h
    ${CMAKE_SOURCE_DIR}/models/*.h
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/mvr/*.h
    ${CMAKE_SOURCE_DIR}/viewer3d/*.h
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

# Mark as GUI application (no console)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

# Include project directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/mvr
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/viewer3d
    ${CMAKE_SOURCE_DIR}/resources
    ${NANOVG_INCLUDE_DIR}
)

# Link required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${wxWidgets_LIBRARIES}
    tinyxml2::tinyxml2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    CURL::libcurl
    nanovg::nanovg
)

# Copy runtime resources (e.g. application icon) next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources)
