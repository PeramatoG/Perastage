cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0074 NEW)

project(Perastage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# wxWidgets chooses different binary sets based on wxWidgets_USE_DEBUG.
# - Single-config: we can select debug libs based on CMAKE_BUILD_TYPE.
# - Multi-config (Visual Studio): do not force wxWidgets_USE_DEBUG unless explicitly requested.
option(PERASTAGE_FORCE_WXDEBUG "Force linking wxWidgets debug libs" OFF)
if(PERASTAGE_FORCE_WXDEBUG)
    set(wxWidgets_USE_DEBUG ON)
elseif(NOT CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(wxWidgets_USE_DEBUG ON)
    else()
        set(wxWidgets_USE_DEBUG OFF)
    endif()
endif()
set(wxWidgets_USE_UNICODE ON)

if(CMAKE_CONFIGURATION_TYPES)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
endif()

# Find required packages
find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(GLEW REQUIRED)
find_package(nanovg CONFIG REQUIRED)
find_package(podofo CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

function(perastage_force_linker_language target_name language)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_aliased ${target_name} ALIASED_TARGET)
    if(_aliased)
        set(_tgt ${_aliased})
    else()
        set(_tgt ${target_name})
    endif()

    # Some package configs define targets (imported or not) without a linker language.
    # This is required for generator expressions like $<TARGET_RUNTIME_DLLS:...>.
    set_property(TARGET ${_tgt} PROPERTY LINKER_LANGUAGE ${language})

    get_target_property(_is_imported ${_tgt} IMPORTED)
    if(_is_imported)
        set_property(TARGET ${_tgt} PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES ${language})

        # Apply for any declared import configurations and also for common configs.
        get_target_property(_import_configs ${_tgt} IMPORTED_CONFIGURATIONS)
        set(_all_configs ${_import_configs} Debug Release RelWithDebInfo MinSizeRel)
        list(REMOVE_DUPLICATES _all_configs)

        foreach(_cfg IN LISTS _all_configs)
            string(TOUPPER "${_cfg}" _cfg_uc)
            set_property(
                TARGET ${_tgt}
                PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_${_cfg_uc} ${language}
            )
        endforeach()
    endif()
endfunction()

# Ensure CMake can generate link rules and runtime dependency rules for these targets.
perastage_force_linker_language(CURL::libcurl C)
perastage_force_linker_language(CURL::libcurl_shared C)
perastage_force_linker_language(GLEW::GLEW C)
perastage_force_linker_language(podofo::podofo CXX)
perastage_force_linker_language(podofo::podofo_shared CXX)
perastage_force_linker_language(tinyxml2::tinyxml2 CXX)
perastage_force_linker_language(nanovg::nanovg C)
perastage_force_linker_language(ZLIB::ZLIB C)

# Gather source files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/models/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/core/layouts/*.cpp
    ${CMAKE_SOURCE_DIR}/core/print/*.cpp
    ${CMAKE_SOURCE_DIR}/mvr/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer3d/*.cpp
    ${CMAKE_SOURCE_DIR}/viewer2d/*.cpp
)

# Explicitly ensure markdown conversion source is included
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/core/markdown.cpp)
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/core/uuidutils.cpp)

# On Windows, include application icon resources
if(WIN32)
    list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/resources/Perastage.rc)
endif()

# Optional: gather headers for IDEs
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/gui/*.h
    ${CMAKE_SOURCE_DIR}/models/*.h
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/core/layouts/*.h
    ${CMAKE_SOURCE_DIR}/core/print/*.h
    ${CMAKE_SOURCE_DIR}/mvr/*.h
    ${CMAKE_SOURCE_DIR}/viewer3d/*.h
    ${CMAKE_SOURCE_DIR}/viewer2d/*.h
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
    ZLIB::ZLIB
)



function(perastage_append_runtime_dlls out_var target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_alias ${target_name} ALIASED_TARGET)
    if(_alias)
        set(target_name ${_alias})
    endif()

    get_target_property(_type ${target_name} TYPE)
    if(_type STREQUAL "SHARED_LIBRARY"
        OR _type STREQUAL "MODULE_LIBRARY"
        OR _type STREQUAL "EXECUTABLE")
        list(APPEND ${out_var} $<TARGET_RUNTIME_DLLS:${target_name}>)
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endif()
endfunction()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources DESTINATION .)
if(EXISTS "${CMAKE_SOURCE_DIR}/library")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/library DESTINATION .)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/licenses")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/licenses DESTINATION .)
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE.txt
    ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    ${CMAKE_SOURCE_DIR}/help.md
    DESTINATION .
    OPTIONAL
)

if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    set(PERASTAGE_RUNTIME_DLLS $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>)

    install(FILES ${PERASTAGE_RUNTIME_DLLS} DESTINATION .)
    install(FILES ${PERASTAGE_RUNTIME_DLLS} DESTINATION . COMPONENT Runtime)

    # wxWidgets libraries may not appear in TARGET_RUNTIME_DLLS when discovered
    # via wxWidgets_USE_FILE, so derive DLL locations from linked libraries too.
    set(PERASTAGE_WX_SEARCH_DIRS "")
    foreach(wx_dir IN LISTS wxWidgets_LIBRARY_DIRS)
        if(wx_dir AND IS_DIRECTORY "${wx_dir}")
            list(APPEND PERASTAGE_WX_SEARCH_DIRS "${wx_dir}")
        endif()
    endforeach()
    foreach(wx_lib IN LISTS wxWidgets_LIBRARIES)
        if(EXISTS "${wx_lib}")
            get_filename_component(wx_lib_dir "${wx_lib}" DIRECTORY)
            if(IS_DIRECTORY "${wx_lib_dir}")
                list(APPEND PERASTAGE_WX_SEARCH_DIRS "${wx_lib_dir}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_WX_SEARCH_DIRS)

    set(PERASTAGE_WX_DLL_PATTERNS "")
    foreach(wx_dir IN LISTS PERASTAGE_WX_SEARCH_DIRS)
        list(APPEND PERASTAGE_WX_DLL_PATTERNS
            "${wx_dir}/wxmsw*.dll"
            "${wx_dir}/wxbase*.dll"
            "${wx_dir}/nwxmsw*.dll"
            "${wx_dir}/nwxbase*.dll")
        get_filename_component(wx_bin_dir "${wx_dir}/../bin" ABSOLUTE)
        list(APPEND PERASTAGE_WX_DLL_PATTERNS
            "${wx_bin_dir}/wxmsw*.dll"
            "${wx_bin_dir}/wxbase*.dll"
            "${wx_bin_dir}/nwxmsw*.dll"
            "${wx_bin_dir}/nwxbase*.dll")
    endforeach()
    list(REMOVE_ITEM PERASTAGE_WX_DLL_PATTERNS "")
    list(REMOVE_DUPLICATES PERASTAGE_WX_DLL_PATTERNS)

    set(PERASTAGE_WX_RUNTIME_DLLS "")
    foreach(wx_pattern IN LISTS PERASTAGE_WX_DLL_PATTERNS)
        file(GLOB PERASTAGE_WX_GLOB CONFIGURE_DEPENDS "${wx_pattern}")
        list(APPEND PERASTAGE_WX_RUNTIME_DLLS ${PERASTAGE_WX_GLOB})
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_WX_RUNTIME_DLLS)

    if(PERASTAGE_WX_RUNTIME_DLLS)
        set(PERASTAGE_WX_DEBUG_DLLS ${PERASTAGE_WX_RUNTIME_DLLS})
        set(PERASTAGE_WX_RELEASE_DLLS ${PERASTAGE_WX_RUNTIME_DLLS})
        list(FILTER PERASTAGE_WX_DEBUG_DLLS INCLUDE REGEX ".*(d_vc_|ud_vc_).*\\.dll$")
        list(FILTER PERASTAGE_WX_RELEASE_DLLS EXCLUDE REGEX ".*(d_vc_|ud_vc_).*\\.dll$")

        if(CMAKE_CONFIGURATION_TYPES)
            install(FILES ${PERASTAGE_WX_DEBUG_DLLS} DESTINATION . CONFIGURATIONS Debug)
            install(FILES ${PERASTAGE_WX_RELEASE_DLLS} DESTINATION . CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
        elseif(wxWidgets_USE_DEBUG)
            install(FILES ${PERASTAGE_WX_DEBUG_DLLS} DESTINATION .)
        else()
            install(FILES ${PERASTAGE_WX_RELEASE_DLLS} DESTINATION .)
        endif()
    endif()

    # Gather runtime DLLs from imported dependencies to avoid missing third-party libraries.
    set(PERASTAGE_DEP_RUNTIME_DLLS "")
    foreach(dep IN LISTS PERASTAGE_IMPORT_DEPS)
        perastage_append_runtime_dlls(PERASTAGE_DEP_RUNTIME_DLLS ${dep})
    endforeach()
    list(REMOVE_DUPLICATES PERASTAGE_DEP_RUNTIME_DLLS)

    if(CMAKE_CONFIGURATION_TYPES)
        install(FILES ${PERASTAGE_DEP_RUNTIME_DLLS} DESTINATION . CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
        install(FILES ${PERASTAGE_DEP_RUNTIME_DLLS} DESTINATION . CONFIGURATIONS Debug)
    else()
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(FILTER PERASTAGE_DEP_RUNTIME_DLLS EXCLUDE REGEX ".*(d_vc_|ud_vc_).*\\.dll$|.*d\\.dll$")
        endif()
        install(FILES ${PERASTAGE_DEP_RUNTIME_DLLS} DESTINATION .)
    endif()
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(PERASTAGE_STAGE_PREFIX "${CMAKE_SOURCE_DIR}/out/install/$<CONFIG>")
    set(PERASTAGE_STAGE_CONFIG --config $<CONFIG>)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/install/$<CONFIG>/symbols")
else()
    set(PERASTAGE_STAGE_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(PERASTAGE_STAGE_CONFIG)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_INSTALL_PREFIX}/symbols")
endif()

add_custom_target(perastage_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PERASTAGE_STAGE_PREFIX}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${PERASTAGE_STAGE_PREFIX}" ${PERASTAGE_STAGE_CONFIG}
    DEPENDS ${PROJECT_NAME}
)

if(WIN32 AND MSVC)
    add_custom_target(perastage_symbols
        COMMAND ${CMAKE_COMMAND}
            -DPDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>
            -DPDB_DESTINATION="${PERASTAGE_SYMBOLS_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopySymbols.cmake"
        DEPENDS ${PROJECT_NAME}
    )
endif()

# Copy runtime resources next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/help.md $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/licenses $<TARGET_FILE_DIR:${PROJECT_NAME}>/licenses
)

set(LIBRARY_SUBDIRS fixtures trusses misc "scene objects" projects)
foreach(subdir IN LISTS LIBRARY_SUBDIRS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/library/${subdir}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/library/${subdir}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/library/${subdir}"
        )
    endif()
endforeach()
