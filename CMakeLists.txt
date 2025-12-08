cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

project(Perastage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(GLEW REQUIRED)
find_package(nanovg CONFIG REQUIRED)
find_package(podofo CONFIG REQUIRED)

# Gather source files
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/models/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/mvr/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer3d/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer2d/*.cpp
)

# Explicitly ensure markdown conversion source is included
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/core/markdown.cpp)

# On Windows, include application icon resources
if (WIN32)
    list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/resources/Perastage.rc)
endif()

# Optional: gather headers for IDEs
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/gui/*.h
    ${CMAKE_SOURCE_DIR}/models/*.h
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/mvr/*.h
    ${CMAKE_SOURCE_DIR}/viewer3d/*.h
    ${CMAKE_SOURCE_DIR}/viewer2d/*.h
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

# Mark as GUI application (no console)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

# Include project directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/mvr
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/viewer3d
    ${CMAKE_SOURCE_DIR}/viewer2d
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
    podofo::podofo
)

enable_testing()
add_subdirectory(tests)

# Copy runtime resources (e.g. application icon) next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources)

# Copy the help file next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/help.md $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Copy license information next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/licenses $<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses)

# Ensure library directories are present alongside the executable
set(LIBRARY_SUBDIRS fixtures trusses misc "scene objects" projects)
foreach(subdir IN LISTS LIBRARY_SUBDIRS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}")
    if (EXISTS "${CMAKE_SOURCE_DIR}/library/${subdir}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/library/${subdir}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}")
    endif()
endforeach()
