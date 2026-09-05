# Stage build-tree runtime assets beside the application on each supported platform.
if(APPLE)
    set(PERASTAGE_RUNTIME_ASSET_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
else()
    set(PERASTAGE_RUNTIME_ASSET_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()

# Copy runtime resources to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERASTAGE_RUNTIME_ASSET_DIR}
)

if(APPLE)
    # Copy resource files directly into Contents/Resources for macOS bundle consistency.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources
                ${PERASTAGE_RUNTIME_ASSET_DIR}
    )
else()
    # Copy resource files into a dedicated resources/ directory on non-macOS platforms.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources
                ${PERASTAGE_RUNTIME_ASSET_DIR}/resources
    )
endif()

if(PERASTAGE_ENABLE_LOCALIZATION)
    foreach(PERASTAGE_TRANSLATION_LANGUAGE IN LISTS PERASTAGE_TRANSLATION_LANGUAGES)
        if(APPLE)
            set(PERASTAGE_RUNTIME_LOCALE_DIR "${PERASTAGE_RUNTIME_ASSET_DIR}/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        else()
            set(PERASTAGE_RUNTIME_LOCALE_DIR "${PERASTAGE_RUNTIME_ASSET_DIR}/resources/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES")
        endif()
        set(PERASTAGE_RUNTIME_MO "${PERASTAGE_GENERATED_LOCALE_DIR}/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES/perastage.mo")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_MO=${PERASTAGE_RUNTIME_MO}"
                    "-DDESTINATION_MO=${PERASTAGE_RUNTIME_LOCALE_DIR}/perastage.mo"
                    -P "${CMAKE_SOURCE_DIR}/cmake/PerastageCopyRuntimeCatalog.cmake"
            COMMAND "${CMAKE_COMMAND}"
                    "-DEXPECTED_FILE=${PERASTAGE_RUNTIME_LOCALE_DIR}/perastage.mo"
                    -P "${CMAKE_SOURCE_DIR}/cmake/PerastageVerifyFileExists.cmake"
            VERBATIM
        )
    endforeach()
endif()

# Copy help documentation to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/help.md ${PERASTAGE_RUNTIME_ASSET_DIR}
)

# Copy license files to the platform runtime asset directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE.txt ${PERASTAGE_RUNTIME_ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md ${PERASTAGE_RUNTIME_ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/licenses ${PERASTAGE_RUNTIME_ASSET_DIR}/licenses
)

set(LIBRARY_SUBDIRS fixtures trusses misc scene_objects projects default_layouts hoists)
foreach(subdir IN LISTS LIBRARY_SUBDIRS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PERASTAGE_RUNTIME_ASSET_DIR}/library/${subdir}"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/library/${subdir}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/library/${subdir}"
                    "${PERASTAGE_RUNTIME_ASSET_DIR}/library/${subdir}"
        )
    endif()
endforeach()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}"
            "${PERASTAGE_RUNTIME_ASSET_DIR}/library/fixtures/Dummy 1ch.gdtf"
)
