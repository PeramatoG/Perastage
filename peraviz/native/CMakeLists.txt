cmake_minimum_required(VERSION 3.21)

project(PeravizNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PERAVIZ_GODOT_PROJECT_DIR "${CMAKE_SOURCE_DIR}/peraviz" CACHE PATH "Path to the Peraviz Godot project")
set(GODOT_CPP_DIR "" CACHE PATH "Path to a local godot-cpp checkout")
option(PERAVIZ_FETCH_GODOT_CPP "Fetch godot-cpp automatically when GODOT_CPP_DIR is not provided" ON)

if(NOT TARGET godot-cpp)
    if(GODOT_CPP_DIR)
        if(EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
            add_subdirectory("${GODOT_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp")
        else()
            message(FATAL_ERROR "GODOT_CPP_DIR was provided but no CMakeLists.txt was found: ${GODOT_CPP_DIR}")
        endif()
    elseif(PERAVIZ_FETCH_GODOT_CPP)
        include(FetchContent)
        FetchContent_Declare(
            godot-cpp
            GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
            GIT_TAG godot-4.2.2-stable
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(godot-cpp)
    else()
        message(FATAL_ERROR "godot-cpp is required. Set GODOT_CPP_DIR or enable PERAVIZ_FETCH_GODOT_CPP.")
    endif()
endif()


if(EXISTS "${CMAKE_SOURCE_DIR}/models/matrixutils.h")
    set(PERASTAGE_ROOT_DIR "${CMAKE_SOURCE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../models/matrixutils.h")
    get_filename_component(PERASTAGE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
else()
    message(FATAL_ERROR "Cannot locate Perastage root (models/matrixutils.h not found).")
endif()

if(NOT tinyxml2_FOUND)
    find_package(tinyxml2 CONFIG QUIET)
endif()
if(NOT TARGET tinyxml2::tinyxml2)
    find_path(TINYXML2_INCLUDE_DIR tinyxml2.h)
    find_library(TINYXML2_LIBRARY tinyxml2)
    if(NOT TINYXML2_INCLUDE_DIR OR NOT TINYXML2_LIBRARY)
        message(FATAL_ERROR "tinyxml2 not found. Install development package or provide tinyxml2Config.cmake")
    endif()
    add_library(tinyxml2::tinyxml2 UNKNOWN IMPORTED)
    set_target_properties(tinyxml2::tinyxml2 PROPERTIES
        IMPORTED_LOCATION "${TINYXML2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TINYXML2_INCLUDE_DIR}"
    )
endif()

if(NOT wxWidgets_FOUND)
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    set(_peraviz_wx_libs ${wxWidgets_LIBRARIES})
else()
    if(TARGET wx::core)
        set(_peraviz_wx_libs wx::core wx::base)
    else()
        set(_peraviz_wx_libs ${wxWidgets_LIBRARIES})
    endif()
endif()

add_library(peraviz_native SHARED
    src/hello_world.cpp
    src/mvr_scene_loader.cpp
    src/peraviz_loader.cpp
    src/register_types.cpp
    src/entry.cpp
)

target_include_directories(peraviz_native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${PERASTAGE_ROOT_DIR}/models"
)

target_link_libraries(peraviz_native PRIVATE
    godot-cpp
    tinyxml2::tinyxml2
    ${_peraviz_wx_libs}
)

set_target_properties(peraviz_native PROPERTIES
    OUTPUT_NAME "peraviz_native"
)

if(PERAVIZ_GODOT_PROJECT_DIR)
    set(_peraviz_bin_dir "${PERAVIZ_GODOT_PROJECT_DIR}/bin")
    file(MAKE_DIRECTORY "${_peraviz_bin_dir}")

    set_target_properties(peraviz_native PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_peraviz_bin_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${_peraviz_bin_dir}"
    )

    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" config_upper)
        set_target_properties(peraviz_native PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${_peraviz_bin_dir}"
            LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${_peraviz_bin_dir}"
        )
    endforeach()
endif()
