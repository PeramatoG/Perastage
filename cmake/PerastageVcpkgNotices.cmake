# Collect package-authored vcpkg copyright files for distribution.
set(PERASTAGE_GENERATED_VCPKG_NOTICE_DIR
    "${CMAKE_BINARY_DIR}/generated/licenses/vcpkg")

# Rebuild the generated notice directory from an installed vcpkg share tree.
function(perastage_collect_vcpkg_notices installed_dir target_triplet output_dir)
    set(share_dir "${installed_dir}/${target_triplet}/share")
    file(REMOVE_RECURSE "${output_dir}")
    if(NOT IS_DIRECTORY "${share_dir}")
        return()
    endif()

    file(MAKE_DIRECTORY "${output_dir}")
    file(GLOB copyright_files LIST_DIRECTORIES FALSE "${share_dir}/*/copyright")
    list(SORT copyright_files)
    foreach(copyright_file IN LISTS copyright_files)
        get_filename_component(port_dir "${copyright_file}" DIRECTORY)
        get_filename_component(port_name "${port_dir}" NAME)
        string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" safe_port_name "${port_name}")
        set(destination "${output_dir}/${safe_port_name}.txt")
        if(EXISTS "${destination}")
            message(FATAL_ERROR
                "vcpkg notice filename collision for ${port_name}: ${destination}")
        endif()
        file(COPY_FILE "${copyright_file}" "${destination}" ONLY_IF_DIFFERENT)
    endforeach()
endfunction()

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    perastage_collect_vcpkg_notices(
        "${VCPKG_INSTALLED_DIR}"
        "${VCPKG_TARGET_TRIPLET}"
        "${PERASTAGE_GENERATED_VCPKG_NOTICE_DIR}")
endif()
