cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

project(Perastage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages via vcpkg using CONFIG mode
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base aui)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(OpenGL REQUIRED)

# Gather source files
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/models/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/mvr/*.cpp
    ${CMAKE_SOURCE_DIR}/render/*.cpp
)

# Optional: gather headers for IDEs
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/gui/*.h
    ${CMAKE_SOURCE_DIR}/models/*.h
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/mvr/*.h
    ${CMAKE_SOURCE_DIR}/render/*.h
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
    ${CMAKE_SOURCE_DIR}/render
)

# Link required libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    wx::core
    wx::base
    wx::aui
    tinyxml2::tinyxml2
    Vulkan::Vulkan
    OpenGL::GL
)
