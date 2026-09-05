# Configure Windows application resources and GUI subsystem behavior.
target_sources(${PROJECT_NAME} PRIVATE
    resources/Perastage.rc
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Preserve MSVC language conformance and Release symbol/link optimization settings.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /Zc:__cplusplus
        "$<$<CONFIG:Release>:/Zi>"
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:/DEBUG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Dbghelp)

# Collect the application PDB in the release-support symbols directory.
if(MSVC)
    set(PERASTAGE_SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/out/symbols/windows")
    add_custom_target(perastage_symbols
        COMMAND ${CMAKE_COMMAND}
            "-DPDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "-DPDB_DESTINATION=${PERASTAGE_SYMBOLS_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopySymbols.cmake"
        DEPENDS ${PROJECT_NAME}
    )
endif()
