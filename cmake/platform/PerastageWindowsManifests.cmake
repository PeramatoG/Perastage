# Embed native executable manifests for both direct and Store Windows builds.
function(perastage_add_windows_manifest target template)
    if(NOT WIN32)
        return()
    endif()
    set(PERASTAGE_WINDOWS_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/${target}.exe.manifest")
    configure_file("${template}" "${PERASTAGE_WINDOWS_MANIFEST}" @ONLY)
    if(MSVC)
        # CMake merges this input with the linker manifest and embeds resource #1.
        target_sources(${target} PRIVATE "${PERASTAGE_WINDOWS_MANIFEST}")
    else()
        # MinGW does not perform MSVC's manifest merge/embedding step.
        set(manifest_resource "${CMAKE_CURRENT_BINARY_DIR}/${target}-manifest.rc")
        configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../resources/WindowsManifest.rc.in"
                       "${manifest_resource}" @ONLY)
        set_source_files_properties("${manifest_resource}" PROPERTIES
            OBJECT_DEPENDS "${PERASTAGE_WINDOWS_MANIFEST}")
        target_sources(${target} PRIVATE "${manifest_resource}")
    endif()
endfunction()
