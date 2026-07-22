set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

enable_testing()

add_library(perastage_test_no_dialog OBJECT windows_no_dialog_runtime.cpp)
link_libraries($<TARGET_OBJECTS:perastage_test_no_dialog>)

function(validate_bash_executable bash_path)
    if(NOT bash_path)
        message(FATAL_ERROR "BASH_EXECUTABLE is required for shell policy tests.")
    endif()
    if(WIN32)
        string(TOLOWER "${bash_path}" bash_path_lower)
        if(bash_path_lower MATCHES "windowsapps[/\\]bash\\.exe$" OR
           bash_path_lower MATCHES "system32[/\\]bash\\.exe$")
            message(FATAL_ERROR "BASH_EXECUTABLE must be Git Bash on Windows, not a WSL launcher: ${bash_path}")
        endif()
        if(NOT bash_path_lower MATCHES "[/\\]git[/\\].*[/\\]bash\\.exe$")
            message(FATAL_ERROR "BASH_EXECUTABLE must come from Git for Windows: ${bash_path}")
        endif()
    endif()
    execute_process(
        COMMAND "${bash_path}" -lc "printf 'cmake-bash-ok\n'"
        RESULT_VARIABLE bash_result
        OUTPUT_VARIABLE bash_output
        ERROR_VARIABLE bash_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT bash_result EQUAL 0 OR NOT bash_output STREQUAL "cmake-bash-ok")
        message(FATAL_ERROR "BASH_EXECUTABLE could not run a trivial shell script: ${bash_path} output='${bash_output}' error='${bash_error}'")
    endif()
endfunction()

function(set_perastage_test_labels test_name)
    set(options)
    set(oneValueArgs LAYER)
    set(multiValueArgs DOMAINS EXTRA)
    cmake_parse_arguments(TEST_LABEL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(labels)
    if(TEST_LABEL_LAYER)
        list(APPEND labels "${TEST_LABEL_LAYER}")
    endif()
    if(TEST_LABEL_DOMAINS)
        list(APPEND labels ${TEST_LABEL_DOMAINS})
    endif()
    if(TEST_LABEL_EXTRA)
        list(APPEND labels ${TEST_LABEL_EXTRA})
    endif()
    if(labels)
        set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
    endif()
endfunction()

function(add_bash_policy_test test_name script_path)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs LABELS ARGS)
    cmake_parse_arguments(POLICY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_test(NAME ${test_name}
             COMMAND "${BASH_EXECUTABLE}" "${script_path}" ${POLICY_ARGS})
    set_tests_properties(${test_name} PROPERTIES
                         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                         ENVIRONMENT "PERASTAGE_TEST_PYTHON=${Python3_EXECUTABLE}")
    if(POLICY_LABELS)
        list(JOIN POLICY_LABELS ";" labels)
        set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
    endif()
endfunction()

function(add_repository_policy_test test_name command_path)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs LABELS ARGS)
    cmake_parse_arguments(POLICY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_test(NAME ${test_name}
             COMMAND "${command_path}" ${POLICY_ARGS})
    set_tests_properties(${test_name} PROPERTIES
                         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                         ENVIRONMENT "PERASTAGE_TEST_PYTHON=${Python3_EXECUTABLE}")
    if(POLICY_LABELS)
        list(JOIN POLICY_LABELS ";" labels)
        set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
    endif()
endfunction()


add_library(perastage_test_diagnostics STATIC
            ../core/apppaths.cpp
            ../core/configservices.cpp
            ../core/diagnostics/DiagnosticPaths.cpp
            ../core/filesystem_path_utils.cpp
            ../core/logger.cpp
            ../core/symbol_cache_manifest.cpp)
target_include_directories(perastage_test_diagnostics PRIVATE
                           ../core
                           ../third_party)

add_library(perastage_test_path_utils STATIC
            ../core/filesystem_path_utils.cpp)
target_include_directories(perastage_test_path_utils PRIVATE ../core)

add_executable(runtime_storage_test
               runtime_storage_test.cpp
               ../core/runtime_storage.cpp
               ../core/logger.cpp)
target_include_directories(runtime_storage_test PRIVATE ../core)
target_link_libraries(runtime_storage_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME RuntimeStorageTest COMMAND runtime_storage_test)


add_library(perastage_test_active_dictionary_storage STATIC
            ../core/active_dictionary_storage.cpp
            ../core/file_import_utils.cpp
            ../core/filesystem_path_utils.cpp)
target_include_directories(perastage_test_active_dictionary_storage PRIVATE ../core ../third_party)

add_library(perastage_test_gdtf_archive_reader STATIC
            ../core/gdtf_archive_reader.cpp)
target_include_directories(perastage_test_gdtf_archive_reader PRIVATE ../core)
target_link_libraries(perastage_test_gdtf_archive_reader PRIVATE ${wxWidgets_LIBRARIES})

find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
    validate_bash_executable("${BASH_EXECUTABLE}")
    add_bash_policy_test(DocsPerastageTreeModules ${CMAKE_CURRENT_SOURCE_DIR}/check_perastage_tree_modules.sh)
    add_bash_policy_test(GuiNoConfigManagerGet ${CMAKE_CURRENT_SOURCE_DIR}/check_no_configmanager_get_in_gui.sh)
    add_bash_policy_test(OpenGLLifecycleCentralized ${CMAKE_CURRENT_SOURCE_DIR}/check_opengl_lifecycle_centralized.sh)
    add_bash_policy_test(Layout2DRasterizationBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_2d_rasterization_boundary.sh)
    add_bash_policy_test(Layout2DViewCaptureBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_2d_view_capture_boundary.sh)
    add_bash_policy_test(Viewer2DStateOwnershipBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_viewer2d_state_ownership_boundary.sh)
    add_bash_policy_test(Viewer2DRenderToRGBAFboBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_viewer2d_render_to_rgba_fbo_boundary.sh)
    add_bash_policy_test(Viewer2DFboCaptureDiagnostics ${CMAKE_CURRENT_SOURCE_DIR}/check_viewer2d_fbo_capture_diagnostics.sh)
    add_bash_policy_test(LayoutPreviewFailureDiagnostics ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_preview_failure_diagnostics.sh)
    add_bash_policy_test(LayoutProjectLoadCacheReset ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_project_load_cache_reset.sh)
    add_bash_policy_test(LayoutViewerFitLifecycle ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_viewer_fit_lifecycle.sh)
    add_bash_policy_test(LibraryUserDataPolicy ${CMAKE_CURRENT_SOURCE_DIR}/check_library_user_data_policy.sh)
    add_bash_policy_test(DictionaryPathsPolicy ${CMAKE_CURRENT_SOURCE_DIR}/check_dictionary_paths.sh)
    add_bash_policy_test(GdtfMetadataSummaryBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_metadata_summary_boundary.sh)
    add_bash_policy_test(GdtfMetadataPanelBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_metadata_panel_boundary.sh)
    add_bash_policy_test(GdtfReusablePanelsBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_reusable_panels_boundary.sh)
    add_bash_policy_test(GdtfEditorPanelBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_editor_panel_boundary.sh)
    add_bash_policy_test(GdtfEditorCoreBoundary ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_editor_core_boundary.sh)
    add_bash_policy_test(GdtfEditorCheckpoint08ABinding ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_editor_checkpoint08a_binding.sh)
    add_bash_policy_test(GdtfEditorCheckpoint08E2ModeBrowser ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_editor_checkpoint08e2_mode_browser.sh)
    add_bash_policy_test(GdtfEditorCheckpoint08E3WheelAttributeInspector ${CMAKE_CURRENT_SOURCE_DIR}/check_gdtf_editor_checkpoint08e3_wheel_attribute_inspector.sh)
    add_bash_policy_test(ProjectRestoreImportOptions ${CMAKE_CURRENT_SOURCE_DIR}/check_project_restore_import_options.sh)
    add_bash_policy_test(SecureStoreBuildPolicy ${CMAKE_CURRENT_SOURCE_DIR}/check_securestore_build_policy.sh LABELS secure-store security release-gate)
    add_bash_policy_test(WindowsNinjaX64Policy ${CMAKE_CURRENT_SOURCE_DIR}/check_windows_ninja_x64_policy.sh LABELS windows x64 release-gate)
    add_bash_policy_test(CiCMakeLanguagePolicy ${CMAKE_CURRENT_SOURCE_DIR}/check_ci_cmake_language_policy.sh LABELS ci cmake release-gate)
    add_bash_policy_test(ReleaseGatePolicyPortability ${CMAKE_CURRENT_SOURCE_DIR}/check_release_gate_policy_portability.sh LABELS ci policy release-gate)
    add_bash_policy_test(PythonResolvedInterpreterPolicy ${CMAKE_CURRENT_SOURCE_DIR}/check_python_resolved_interpreter_policy.sh LABELS standard-strict policy)
endif()

if(Python3_Interpreter_FOUND)
    add_repository_policy_test(DocsLinks ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_docs_links.py)
    add_repository_policy_test(MacosSdkCacheGuard ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_macos_sdk_cache_guard.py)
    add_repository_policy_test(WxFilesystemPathBoundaries ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_wx_filesystem_path_boundaries.py)
    add_repository_policy_test(ReleaseWorkflowsUntouched ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_release_workflows_untouched.py)
    add_repository_policy_test(BashTestRegistration ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_bash_test_registration.py)
    add_repository_policy_test(WindowsDebugToolPreflight ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_windows_debug_tool_preflight.py)
    add_repository_policy_test(CMakeToolchainValidator ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_validate_cmake_toolchain.py)
    add_repository_policy_test(CiCMakeLanguagePolicyFixtures ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_ci_cmake_language_policy_fixtures.py --bash ${BASH_EXECUTABLE})
    add_repository_policy_test(PolicyWorkingDirectories ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_policy_working_directories.py --bash ${BASH_EXECUTABLE})
    add_repository_policy_test(MissingRipgrepBehavior ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_missing_ripgrep_behavior.py --bash ${BASH_EXECUTABLE})
    add_repository_policy_test(CtestFailureSummary ${Python3_EXECUTABLE}
                               ARGS ${CMAKE_CURRENT_SOURCE_DIR}/check_summarize_ctest_failures.py)
endif()

link_libraries(perastage_test_active_dictionary_storage)

include_directories(
    .
    ../core
    ../core/layouts
    ../core/print
    ../models
    ../mvr
    ../gui
    ../third_party
    ../viewer2d
    ../viewer2d/pdf
    ../viewer3d
    ../viewer3d/interfaces
    ../viewer3d/resources
    ../viewer3d/culling
    ../viewer3d/labels
    ../viewer3d/picking
    ../viewer3d/render
)



add_executable(localization_app_language_test
               localization_app_language_test.cpp
               ../core/localization/app_language.cpp
               ../core/localization/localization_manager.cpp
               localization_diagnostic_logger_stub.cpp)
target_include_directories(localization_app_language_test PRIVATE ../core ../third_party)
target_compile_definitions(localization_app_language_test PRIVATE
                           PERASTAGE_ENABLE_LOCALIZATION=$<BOOL:${PERASTAGE_ENABLE_LOCALIZATION}>)
target_link_libraries(localization_app_language_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME LocalizationAppLanguage COMMAND localization_app_language_test)


if(PERASTAGE_ENABLE_LOCALIZATION)
    add_executable(localization_catalog_integration_test
                   localization_catalog_integration_test.cpp
                   ../gui/localized_unit_labels.cpp
                   ../core/localization/app_language.cpp
                   ../core/localization/localization_manager.cpp
                   localization_diagnostic_logger_stub.cpp)
    target_include_directories(localization_catalog_integration_test PRIVATE ../core ../gui ../third_party)
    target_compile_definitions(localization_catalog_integration_test PRIVATE
                               PERASTAGE_ENABLE_LOCALIZATION=$<BOOL:${PERASTAGE_ENABLE_LOCALIZATION}>)
    target_link_libraries(localization_catalog_integration_test PRIVATE ${wxWidgets_LIBRARIES})
    add_dependencies(localization_catalog_integration_test perastage_translations)
    add_test(NAME LocalizationCatalogIntegration COMMAND localization_catalog_integration_test)
    set_tests_properties(LocalizationCatalogIntegration PROPERTIES
                         ENVIRONMENT "PERASTAGE_LOCALE_ROOT=${PERASTAGE_GENERATED_LOCALE_DIR}")
    add_test(NAME GettextHelperRegression
             COMMAND "${CMAKE_COMMAND}"
                     "-DHELPER_SCRIPT=${CMAKE_SOURCE_DIR}/cmake/PerastageCompileGettextCatalog.cmake"
                     "-DMSGFMT_EXECUTABLE=${PERASTAGE_MSGFMT_EXECUTABLE}"
                     "-DGETTEXT_BIN_DIR=${PERASTAGE_MSGFMT_DIRECTORY}"
                     "-DINPUT_PO=${CMAKE_SOURCE_DIR}/resources/locale/es/LC_MESSAGES/perastage.po"
                     "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
                     -P "${CMAKE_CURRENT_SOURCE_DIR}/PerastageGettextHelperRegression.cmake")
endif()


add_executable(dictionary_editor_state_test dictionary_editor_state_test.cpp)
target_include_directories(dictionary_editor_state_test PRIVATE ../gui)
add_test(NAME DictionaryEditorState COMMAND dictionary_editor_state_test)


add_executable(wx_path_utils_test wx_path_utils_test.cpp
               ../core/wx_path_utils.cpp)
target_include_directories(wx_path_utils_test PRIVATE ../core)
target_link_libraries(wx_path_utils_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME WxPathUtilsRoundTrip COMMAND wx_path_utils_test)

add_executable(platform_locale_test
               platform_locale_test.cpp
               ../core/platform_locale.cpp)
target_include_directories(platform_locale_test PRIVATE ../core)
add_test(NAME PlatformLocaleSetup COMMAND platform_locale_test)

add_executable(trussloader_validation_test
               trussloader_validation_test.cpp
               build_info_stub.cpp
               ../core/apppaths.cpp
               ../core/diagnostics/DiagnosticPaths.cpp
               ../core/filesystem_path_utils.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../models/truss.cpp)
target_include_directories(trussloader_validation_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(trussloader_validation_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME TrussLoaderValidation COMMAND trussloader_validation_test)

add_executable(truss_creation_source_test
               truss_creation_source_test.cpp
               ../gui/truss_creation_source.cpp
               ../core/filesystem_path_utils.cpp
               ../models/truss.cpp)
target_include_directories(truss_creation_source_test PRIVATE
                           . ../core ../gui ../models ../third_party)
add_test(NAME TrussCreationSource COMMAND truss_creation_source_test)

add_executable(rigging_weight_validation_test
               rigging_weight_validation_test.cpp)
target_include_directories(rigging_weight_validation_test PRIVATE ../gui)
add_test(NAME RiggingWeightValidation COMMAND rigging_weight_validation_test)

add_executable(preview_resource_test
               preview_resource_test.cpp
               ../gui/preview_resource.cpp
               ../core/filesystem_path_utils.cpp
               ../models/truss.cpp)
target_include_directories(preview_resource_test PRIVATE
                           . ../core ../gui ../models)
add_test(NAME PreviewResourceDispatch COMMAND preview_resource_test)

add_executable(mvr_merge_analyzer_applier_test
               mvr_merge_analyzer_applier_test.cpp
               ../mvr/mvr_merge_analyzer.cpp
               ../mvr/mvr_merge_applier.cpp
               ../mvr/mvr_merge_resource_rewriter.cpp
               ../mvr/mvrscenemerger.cpp
               ../core/diagnostics/DiagnosticPaths.cpp
               ../core/filesystem_path_utils.cpp
               ../core/logger.cpp
               ../core/runtime_storage.cpp
               ../core/uuidutils.cpp
               ../models/mvrscene.cpp)
add_test(NAME MvrMergeAnalyzerApplier COMMAND mvr_merge_analyzer_applier_test)

add_executable(matrixutils_test matrixutils_test.cpp)
target_include_directories(matrixutils_test PRIVATE ../models ../core)
add_test(NAME MatrixUtilsParsingAndComposition COMMAND matrixutils_test)

add_executable(scene_grouping_test scene_grouping_test.cpp
    ../core/scene_grouping.cpp
    ../core/uuidutils.cpp
    ../models/mvrscene.cpp
)
target_include_directories(scene_grouping_test PRIVATE ../core ../models)
add_test(NAME SceneGroupingPreservesTransforms COMMAND scene_grouping_test)

add_executable(magnet_snap_test magnet_snap_test.cpp
    ../core/magnet_snap.cpp
    ../core/scene_grouping.cpp
    ../core/uuidutils.cpp
    ../models/mvrscene.cpp
)
target_include_directories(magnet_snap_test PRIVATE ../core ../models)
add_test(NAME MagnetSnapCore COMMAND magnet_snap_test)

add_executable(rdp_simplifier_test
               rdp_simplifier_test.cpp
               ../core/geometry/RdpSimplifier.cpp)
target_include_directories(rdp_simplifier_test PRIVATE ../core)
add_test(NAME RdpSimplifierGeometry COMMAND rdp_simplifier_test)

add_executable(pdf_font_metrics_test
               pdf_font_metrics_test.cpp
               ../viewer2d/pdf/font_metrics.cpp)
add_test(NAME PdfFontMetricsEncoding COMMAND pdf_font_metrics_test)

add_executable(fixture_label_overrides_reconcile_test
               fixture_label_overrides_reconcile_test.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               mvr_io_stubs.cpp)
target_include_directories(fixture_label_overrides_reconcile_test PRIVATE
                           . ../core ../third_party)
target_link_libraries(fixture_label_overrides_reconcile_test PRIVATE
                      ${wxWidgets_LIBRARIES})
add_test(NAME FixtureLabelOverridesReconcile
         COMMAND fixture_label_overrides_reconcile_test)

add_executable(pdf_writer_test
               pdf_writer_test.cpp
               ../viewer2d/pdf/pdf_draw_commands.cpp
               ../viewer2d/pdf/pdf_font_metrics.cpp
               ../viewer2d/pdf/pdf_objects.cpp
               ../viewer2d/pdf/pdf_writer.cpp)
if(TARGET ZLIB::ZLIB)
    target_link_libraries(pdf_writer_test PRIVATE ZLIB::ZLIB)
endif()
add_test(NAME PdfWriterSerialization COMMAND pdf_writer_test)

add_executable(fixture_table_parser_test
               fixture_table_parser_test.cpp
               ../gui/fixturetable/fixture_table_parser.cpp)
target_include_directories(fixture_table_parser_test PRIVATE .. ../gui)
target_link_libraries(fixture_table_parser_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME FixtureTableParser COMMAND fixture_table_parser_test)

add_executable(mainwindow_cli_shortcut_router_test
               mainwindow_cli_shortcut_router_test.cpp
               ../gui/mainwindow_cli_shortcut_router.cpp
               ../gui/shortcut_registry.cpp)
target_include_directories(mainwindow_cli_shortcut_router_test PRIVATE .. ../gui)
add_test(NAME MainWindowCliShortcutRouter COMMAND mainwindow_cli_shortcut_router_test)

add_executable(console_command_parser_test
               console_command_parser_test.cpp
               ../gui/console_command_parser.cpp)
target_include_directories(console_command_parser_test PRIVATE .. ../gui ../core ../models)
add_test(NAME ConsoleCommandParser COMMAND console_command_parser_test)

add_executable(editable_focus_utils_test
               editable_focus_utils_test.cpp
               ../gui/editable_focus_utils.cpp)
target_include_directories(editable_focus_utils_test PRIVATE .. ../gui)
target_link_libraries(editable_focus_utils_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME EditableFocusUtils COMMAND editable_focus_utils_test)

add_executable(shortcut_registry_test
               shortcut_registry_test.cpp
               ../gui/shortcut_registry.cpp)
target_include_directories(shortcut_registry_test PRIVATE .. ../gui)
add_test(NAME ShortcutRegistry COMMAND shortcut_registry_test)


add_executable(ui_unit_utils_test
               ui_unit_utils_test.cpp
               ../core/units/units.cpp
               ../core/units/unit_label_utils.cpp)
target_include_directories(ui_unit_utils_test PRIVATE ../core)
add_test(NAME UiUnitUtilsRoundTrip COMMAND ui_unit_utils_test)

add_executable(gdtf_canonicalizer_test
               gdtf_canonicalizer_test.cpp
               build_info_stub.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp)
target_include_directories(gdtf_canonicalizer_test PRIVATE ../core ../third_party)
target_link_libraries(gdtf_canonicalizer_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfCanonicalizerExportRules COMMAND gdtf_canonicalizer_test)


add_executable(gdtf_source_fingerprint_test
               gdtf_source_fingerprint_test.cpp
               ../gui/gdtf_source_fingerprint.cpp
               ../core/filesystem_path_utils.cpp)
target_include_directories(gdtf_source_fingerprint_test PRIVATE ../gui ../core)
add_test(NAME GdtfSourceFingerprint COMMAND gdtf_source_fingerprint_test)

add_executable(gdtf_metadata_summary_test
               gdtf_metadata_summary_test.cpp
               ../core/gdtf_archive_reader.cpp
               ../core/gdtf_description_reader.cpp
               ../core/gdtf_metadata_summary.cpp)
target_include_directories(gdtf_metadata_summary_test PRIVATE ../core)
target_link_libraries(gdtf_metadata_summary_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfMetadataSummary COMMAND gdtf_metadata_summary_test)

add_executable(gdtf_read_services_test
               gdtf_read_services_test.cpp
               ../core/gdtf_archive_reader.cpp
               ../core/gdtf_description_reader.cpp
               ../core/gdtf_metadata_summary.cpp)
target_include_directories(gdtf_read_services_test PRIVATE ../core)
target_link_libraries(gdtf_read_services_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfReadServices COMMAND gdtf_read_services_test)
set_perastage_test_labels(GdtfReadServices LAYER tolerant-recovery DOMAINS gdtf zip)

add_executable(gdtf_test_fixture_builder_test
               gdtf_test_fixture_builder_test.cpp
               support/gdtf_test_fixture_builder.cpp)
target_include_directories(gdtf_test_fixture_builder_test PRIVATE .)
target_link_libraries(gdtf_test_fixture_builder_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfTestFixtureBuilder COMMAND gdtf_test_fixture_builder_test)

add_test(NAME DummyGdtfFallbackCompliance
         COMMAND ${CMAKE_COMMAND} -E env "PERASTAGE_TEST_PYTHON=${Python3_EXECUTABLE}"
                 ${CMAKE_CURRENT_SOURCE_DIR}/check_dummy_gdtf_fallback.sh
                 "${CMAKE_BINARY_DIR}/generated/library/fixtures/Dummy 1ch.gdtf")
set_perastage_test_labels(DummyGdtfFallbackCompliance LAYER standard-strict DOMAINS gdtf packaging)
set_perastage_test_labels(GdtfTestFixtureBuilder LAYER standard-strict DOMAINS gdtf zip)
add_executable(gdtf_mode_channel_browser_test
               gdtf_mode_channel_browser_test.cpp
               ../core/gdtf/gdtf_mode_channel_browser.cpp)
target_include_directories(gdtf_mode_channel_browser_test PRIVATE ../core)
target_link_libraries(gdtf_mode_channel_browser_test PRIVATE tinyxml2::tinyxml2)
add_test(NAME GdtfModeChannelBrowser COMMAND gdtf_mode_channel_browser_test)

add_executable(gdtf_mode_browser_presenter_test
               gdtf_mode_browser_presenter_test.cpp
               ../gui/gdtf/gdtf_mode_browser_presenter.cpp
               ../core/gdtf/gdtf_mode_channel_browser.cpp)
target_include_directories(gdtf_mode_browser_presenter_test PRIVATE ../core ../gui)
target_link_libraries(gdtf_mode_browser_presenter_test PRIVATE tinyxml2::tinyxml2)
add_test(NAME GdtfModeBrowserPresenter COMMAND gdtf_mode_browser_presenter_test)

add_executable(gdtf_wheel_attribute_inspector_test
               gdtf_wheel_attribute_inspector_test.cpp
               ../core/gdtf/gdtf_color_cie.cpp
               ../core/gdtf/gdtf_wheel_catalog.cpp
               ../core/gdtf/gdtf_dmx_inspector.cpp
               ../core/gdtf/gdtf_mode_channel_browser.cpp)
target_include_directories(gdtf_wheel_attribute_inspector_test PRIVATE ../core)
target_link_libraries(gdtf_wheel_attribute_inspector_test PRIVATE tinyxml2::tinyxml2)
add_test(NAME GdtfWheelAttributeInspector COMMAND gdtf_wheel_attribute_inspector_test)



add_executable(gdtf_fixture_insertion_preparation_test
               gdtf_fixture_insertion_preparation_test.cpp
               ../core/gdtf_archive_reader.cpp
               ../core/gdtf_description_reader.cpp
               ../core/gdtf_fixture_insertion_preparation.cpp)
target_include_directories(gdtf_fixture_insertion_preparation_test PRIVATE ../core)
target_link_libraries(gdtf_fixture_insertion_preparation_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfFixtureInsertionPreparation
         COMMAND gdtf_fixture_insertion_preparation_test)


add_executable(project_fixture_gdtf_apply_adapter_test
               project_fixture_gdtf_apply_adapter_test.cpp
               ../core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp
               ../core/gdtf/editor/gdtf_editor_context.cpp
               ../core/gdtf/editor/gdtf_editable_values.cpp
               ../core/gdtf/editor/gdtf_field_registry.cpp
               ../core/filesystem_path_utils.cpp)
target_include_directories(project_fixture_gdtf_apply_adapter_test PRIVATE .. ../core ../models)
add_test(NAME ProjectFixtureGdtfApplyAdapter COMMAND project_fixture_gdtf_apply_adapter_test)
add_bash_policy_test(GdtfEditorCheckpoint08BFixtureApplyAdapter ${CMAKE_SOURCE_DIR}/tests/check_gdtf_editor_checkpoint08b_fixture_apply_adapter.sh)
set_tests_properties(GdtfEditorCheckpoint08BFixtureApplyAdapter PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_executable(project_truss_gdtf_apply_adapter_test
               project_truss_gdtf_apply_adapter_test.cpp
               ../core/gdtf/editor/project_truss_gdtf_apply_adapter.cpp
               ../core/gdtf/editor/gdtf_editor_context.cpp
               ../core/gdtf/editor/gdtf_editable_values.cpp
               ../core/gdtf/editor/gdtf_field_registry.cpp
               ../core/filesystem_path_utils.cpp)
target_include_directories(project_truss_gdtf_apply_adapter_test PRIVATE .. ../core ../models)
add_test(NAME ProjectTrussGdtfApplyAdapter COMMAND project_truss_gdtf_apply_adapter_test)
add_bash_policy_test(GdtfEditorCheckpoint08CTrussApplyAdapter ${CMAKE_SOURCE_DIR}/tests/check_gdtf_editor_checkpoint08c_truss_apply_adapter.sh)
set_tests_properties(GdtfEditorCheckpoint08CTrussApplyAdapter PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_bash_policy_test(GdtfEditorCheckpoint08DStabilization ${CMAKE_SOURCE_DIR}/tests/check_gdtf_editor_checkpoint08d_stabilization.sh)
set_tests_properties(GdtfEditorCheckpoint08DStabilization PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(gdtf_editor_context_test
               gdtf_editor_context_test.cpp
               ../core/gdtf_archive_reader.cpp
               ../core/gdtf_description_reader.cpp
               ../core/gdtf/editor/gdtf_document.cpp
               ../core/gdtf/editor/gdtf_edit_session.cpp
               ../core/gdtf/editor/gdtf_editable_values.cpp
               ../core/gdtf/editor/gdtf_editor_context.cpp
               ../core/gdtf/editor/gdtf_field_registry.cpp
               ../core/gdtf/editor/project_fixture_gdtf_context.cpp
               ../core/gdtf/editor/project_truss_gdtf_context.cpp
               ../core/gdtf/editor/standalone_gdtf_context.cpp)
target_include_directories(gdtf_editor_context_test PRIVATE .. ../core)
target_link_libraries(gdtf_editor_context_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfEditorContext COMMAND gdtf_editor_context_test)

add_executable(gdtf_fixture_category_test
               gdtf_fixture_category_test.cpp
               ../core/gdtf_fixture_category.cpp)
target_include_directories(gdtf_fixture_category_test PRIVATE ../core)
target_link_libraries(gdtf_fixture_category_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfFixtureCategoryFallback COMMAND gdtf_fixture_category_test)

add_executable(gdtf_dictionary_color_roundtrip_test
               gdtf_dictionary_color_roundtrip_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               mvr_io_stubs.cpp)
target_include_directories(gdtf_dictionary_color_roundtrip_test PRIVATE
                           . ../core ../third_party)
target_link_libraries(gdtf_dictionary_color_roundtrip_test PRIVATE
                      ${wxWidgets_LIBRARIES}
                      tinyxml2::tinyxml2)
add_test(NAME GdtfDictionaryColorRoundtrip COMMAND gdtf_dictionary_color_roundtrip_test)
set_perastage_test_labels(GdtfDictionaryColorRoundtrip LAYER perastage-extension DOMAINS gdtf project)

add_executable(gdtf_dictionary_color_mode_propagation_test
               gdtf_dictionary_color_mode_propagation_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               mvr_io_stubs.cpp)
target_include_directories(gdtf_dictionary_color_mode_propagation_test PRIVATE
                           . ../core ../third_party)
target_link_libraries(gdtf_dictionary_color_mode_propagation_test PRIVATE
                      ${wxWidgets_LIBRARIES}
                      tinyxml2::tinyxml2)
add_test(NAME GdtfDictionaryColorModePropagation
         COMMAND gdtf_dictionary_color_mode_propagation_test)
set_perastage_test_labels(GdtfDictionaryColorModePropagation LAYER perastage-extension DOMAINS gdtf project)

add_executable(dictionary_seed_merge_backup_test
               dictionary_seed_merge_backup_test.cpp
               mvr_io_stubs.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/filesystem_path_utils.cpp
               ../core/logger.cpp
               ../core/runtime_storage.cpp
               ../core/trussdictionary.cpp
               ../models/truss.cpp
               ../models/mvrscene.cpp)
target_include_directories(dictionary_seed_merge_backup_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(dictionary_seed_merge_backup_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME DictionarySeedMergeBackup COMMAND dictionary_seed_merge_backup_test)

add_executable(active_dictionary_workflow_test
               active_dictionary_workflow_test.cpp
               mvr_io_stubs.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/filesystem_path_utils.cpp
               ../core/logger.cpp
               ../core/runtime_storage.cpp
               ../core/trussdictionary.cpp
               ../models/truss.cpp
               ../models/mvrscene.cpp)
target_include_directories(active_dictionary_workflow_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(active_dictionary_workflow_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME ActiveDictionaryWorkflow COMMAND active_dictionary_workflow_test)

add_executable(layout_template_serializer_test
               layout_template_serializer_test.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp)
add_test(NAME LayoutTemplateSerializer COMMAND layout_template_serializer_test)



add_executable(layout_selection_policy_test
               layout_selection_policy_test.cpp
               ../core/layouts/LayoutSelectionPolicy.cpp)
target_include_directories(layout_selection_policy_test PRIVATE
                           ../core/layouts)
add_test(NAME LayoutSelectionPolicy
         COMMAND layout_selection_policy_test)

add_executable(layout_template_package_service_test
               layout_template_package_service_test.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/runtime_storage.cpp
               ../core/filesystem_path_utils.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp)
target_include_directories(layout_template_package_service_test PRIVATE
                           . ../core ../core/layouts ../third_party)
target_link_libraries(layout_template_package_service_test PRIVATE
                      ${wxWidgets_LIBRARIES})
add_test(NAME LayoutTemplatePackageService
         COMMAND layout_template_package_service_test)

add_executable(layout_collection_zorder_test
               layout_collection_zorder_test.cpp
               ../core/layouts/LayoutCollection.cpp)
add_test(NAME LayoutCollectionZOrder COMMAND layout_collection_zorder_test)

add_executable(layout_template_import_name_collision_test
               layout_template_import_name_collision_test.cpp
               configmanager_layoutmanager_stub.cpp
               ../core/layouts/LayoutManager.cpp
    ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/projectutils.cpp
               ../core/logger.cpp
               ../core/library/library_bootstrap.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
    ../core/layouts/LayoutTemplatePackageService.cpp
    ../core/layouts/LayoutSelectionPolicy.cpp
    ../core/runtime_storage.cpp
    ../core/filesystem_path_utils.cpp)
target_link_libraries(layout_template_import_name_collision_test
                      PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME LayoutTemplateImportNameCollision
         COMMAND layout_template_import_name_collision_test)

add_executable(layout_defaults_load_from_empty_config_test
               layout_defaults_load_from_empty_config_test.cpp
               configmanager_layoutmanager_stub.cpp
               ../core/layouts/LayoutManager.cpp
    ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/projectutils.cpp
               ../core/logger.cpp
               ../core/library/library_bootstrap.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
    ../core/layouts/LayoutTemplatePackageService.cpp
    ../core/layouts/LayoutSelectionPolicy.cpp
    ../core/runtime_storage.cpp
    ../core/filesystem_path_utils.cpp)
target_link_libraries(layout_defaults_load_from_empty_config_test
                      PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME LayoutDefaultsLoadFromEmptyConfig
         COMMAND layout_defaults_load_from_empty_config_test)

function(set_library_env TEST_NAME)
    set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "PERASTAGE_LIBRARY_PATH=${CMAKE_SOURCE_DIR}/library")
endfunction()

set(LAYOUT_SOURCES
    ../core/layouts/LayoutManager.cpp
    ../core/layouts/LayoutImageResourceRegistry.cpp
    ../core/layouts/LayoutDefaultsLoader.cpp
    ../core/layouts/LayoutCollection.cpp
    ../core/layouts/LayoutTemplateSerializer.cpp
    ../core/layouts/LayoutTemplatePackageService.cpp
    ../core/layouts/LayoutSelectionPolicy.cpp
    ../core/runtime_storage.cpp
    ../core/filesystem_path_utils.cpp)

if(TARGET podofo::podofo)
    add_executable(pdf_text_test pdf_text_test.cpp ../core/pdftext.cpp)
    target_link_libraries(pdf_text_test PRIVATE podofo::podofo ${wxWidgets_LIBRARIES})
    target_include_directories(pdf_text_test PRIVATE ../core)
    add_test(NAME PdfTextComparison
             COMMAND pdf_text_test
             ${CMAKE_CURRENT_SOURCE_DIR}/data/simple.pdf
             ${CMAKE_CURRENT_SOURCE_DIR}/data/translated.pdf)
    set_library_env(PdfTextComparison)
endif()

add_executable(autopatcher_test autopatcher_test.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp)
target_include_directories(autopatcher_test PRIVATE ../core ../models ../viewer3d)
add_test(NAME AutoPatchTypeGrouping COMMAND autopatcher_test)
set_library_env(AutoPatchTypeGrouping)

add_executable(autopatcher_grouping_test autopatcher_grouping_test.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp)
target_include_directories(autopatcher_grouping_test PRIVATE ../core ../models ../viewer3d)
add_test(NAME AutoPatchGroupIntegrity COMMAND autopatcher_grouping_test)
set_library_env(AutoPatchGroupIntegrity)

add_executable(autopatcher_universe_wrap_test autopatcher_universe_wrap_test.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp)
target_include_directories(autopatcher_universe_wrap_test PRIVATE ../core ../models ../viewer3d)
add_test(NAME AutoPatchUniverseWrap COMMAND autopatcher_universe_wrap_test)
set_library_env(AutoPatchUniverseWrap)

add_executable(save_load_roundtrip_test save_load_roundtrip_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(save_load_roundtrip_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(save_load_roundtrip_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME SaveLoadRoundtrip COMMAND save_load_roundtrip_test)
set_library_env(SaveLoadRoundtrip)

add_executable(project_support_userdata_roundtrip_test project_support_userdata_roundtrip_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(project_support_userdata_roundtrip_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(project_support_userdata_roundtrip_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME ProjectSupportUserDataRoundtrip COMMAND project_support_userdata_roundtrip_test)
set_library_env(ProjectSupportUserDataRoundtrip)

add_executable(truss_path_encoding_regression_test
               truss_path_encoding_regression_test.cpp
               build_info_stub.cpp
               configmanager_layoutmanager_stub.cpp
               ../core/projectutils.cpp
               ../core/apppaths.cpp
               ../core/filesystem_path_utils.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../models/truss.cpp)
target_include_directories(truss_path_encoding_regression_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(truss_path_encoding_regression_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME TrussPathEncodingRegression COMMAND truss_path_encoding_regression_test)

add_executable(rider_truss_dictionary_normalization_test
               rider_truss_dictionary_normalization_test.cpp
               build_info_stub.cpp
               pdftext_stub.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_truss_dictionary_normalization_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_truss_dictionary_normalization_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderTrussDictionaryNormalization COMMAND rider_truss_dictionary_normalization_test)

add_executable(mvr_gdtf_import_matching_test mvr_gdtf_import_matching_test.cpp
               ../mvr/gdtf_catalog_matcher.cpp)
target_include_directories(mvr_gdtf_import_matching_test PRIVATE ../mvr)
add_test(NAME MvrGdtfImportMatching COMMAND mvr_gdtf_import_matching_test)

add_executable(mvr_dmx_address_test mvr_dmx_address_test.cpp
               build_info_stub.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_dmx_address_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_dmx_address_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrDmxAddressConversion COMMAND mvr_dmx_address_test)
set_library_env(MvrDmxAddressConversion)


add_executable(mvr_support_userdata_roundtrip_test mvr_support_userdata_roundtrip_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_support_userdata_roundtrip_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_support_userdata_roundtrip_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrSupportUserDataRoundtrip COMMAND mvr_support_userdata_roundtrip_test)
set_library_env(MvrSupportUserDataRoundtrip)


add_executable(mvr_layer_appearance_userdata_test mvr_layer_appearance_userdata_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_layer_appearance_userdata_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_layer_appearance_userdata_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrLayerAppearanceUserData COMMAND mvr_layer_appearance_userdata_test)
set_library_env(MvrLayerAppearanceUserData)


add_executable(mvr_fixture_category_roundtrip_test mvr_fixture_category_roundtrip_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_fixture_category_roundtrip_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_fixture_category_roundtrip_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrFixtureCategoryRoundtrip COMMAND mvr_fixture_category_roundtrip_test)
set_library_env(MvrFixtureCategoryRoundtrip)

add_executable(mvr_sceneobject_symbol_identity_test mvr_sceneobject_symbol_identity_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../viewer3d/culling/bounds_cache_system.cpp
               ../viewer3d/culling/primitive_bounds_utils.cpp
               ../viewer3d/meshprimitives.cpp
               resource_sync_key_stub.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_sceneobject_symbol_identity_test PRIVATE
                           . ../core ../models ../mvr ../gui ../viewer3d
                           ../viewer3d/culling ../viewer3d/resources ../third_party)
target_link_libraries(mvr_sceneobject_symbol_identity_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrSceneObjectSymbolIdentity COMMAND mvr_sceneobject_symbol_identity_test)
set_library_env(MvrSceneObjectSymbolIdentity)

add_executable(mvr_truss_roundtrip_structure_test mvr_truss_roundtrip_structure_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_truss_roundtrip_structure_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_truss_roundtrip_structure_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrTrussRoundtripStructure COMMAND mvr_truss_roundtrip_structure_test)
set_perastage_test_labels(MvrTrussRoundtripStructure LAYER standard-strict DOMAINS mvr gdtf)
set_library_env(MvrTrussRoundtripStructure)

add_executable(mvr_exporter_compliance_test mvr_exporter_compliance_test.cpp
               build_info_stub.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/layer_service.cpp
               ../core/scene_grouping.cpp
               ../core/utf8_utils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_exporter_compliance_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(mvr_exporter_compliance_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME MvrExporterCompliance COMMAND mvr_exporter_compliance_test)
set_library_env(MvrExporterCompliance)

add_executable(rider_save_roundtrip_test rider_save_roundtrip_test.cpp
               build_info_stub.cpp
               pdftext_stub.cpp
               gdtfloader_stub.cpp
               consolepanel_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/utf8_utils.cpp
               ../core/scene_grouping.cpp
               ../core/layer_service.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/dummyprofilelibrary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_save_roundtrip_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_save_roundtrip_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderSaveRoundtrip COMMAND rider_save_roundtrip_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_basic.txt)
set_perastage_test_labels(RiderSaveRoundtrip LAYER perastage-extension DOMAINS rider project gdtf)
set_library_env(RiderSaveRoundtrip)

add_executable(riderimporter_fixtureid_test rider_import_fixtureid_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(riderimporter_fixtureid_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(riderimporter_fixtureid_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImporterFixtureIds COMMAND riderimporter_fixtureid_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_basic.txt
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_over100.txt)
set_library_env(RiderImporterFixtureIds)

add_executable(rider_import_order_test rider_import_order_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_order_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_order_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImportOrder COMMAND rider_import_order_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixture_order.txt)
set_library_env(RiderImportOrder)

add_executable(rider_import_linear_order_test rider_import_linear_order_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_linear_order_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_linear_order_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImportLinearOrder COMMAND rider_import_linear_order_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixture_linear_order.txt)
set_library_env(RiderImportLinearOrder)

add_executable(rider_import_preserve_existing_test rider_import_preserve_existing_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_preserve_existing_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_preserve_existing_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImportPreserveExisting COMMAND rider_import_preserve_existing_test)
set_library_env(RiderImportPreserveExisting)

add_executable(rider_import_category_test rider_import_category_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_category_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_category_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImportCategory COMMAND rider_import_category_test)
set_library_env(RiderImportCategory)

add_executable(rider_autopatch_test rider_autopatch_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_onech_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_autopatch_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_autopatch_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderAutoPatch COMMAND rider_autopatch_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_basic.txt)
set_library_env(RiderAutoPatch)

add_executable(rider_layer_mode_test rider_layer_mode_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_layer_mode_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_layer_mode_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderLayerMode COMMAND rider_layer_mode_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_layer_mode.txt)
set_library_env(RiderLayerMode)

add_executable(hoist_weight_distribution_test hoist_weight_distribution_test.cpp
               ../core/hoist_weight_distribution.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp)
target_include_directories(hoist_weight_distribution_test PRIVATE
                           ../core ../models ../third_party)
target_link_libraries(hoist_weight_distribution_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME HoistWeightDistribution COMMAND hoist_weight_distribution_test)

add_executable(rider_filter_preview_test rider_filter_preview_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_filter_preview_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_filter_preview_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderFilterPreview COMMAND rider_filter_preview_test)
set_library_env(RiderFilterPreview)

add_executable(rider_comments_test rider_comments_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_comments_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_comments_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderComments COMMAND rider_comments_test)
set_library_env(RiderComments)

add_executable(rider_floor_default_hang_test rider_floor_default_hang_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_floor_default_hang_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_floor_default_hang_test PRIVATE ${wxWidgets_LIBRARIES}
                                                            tinyxml2::tinyxml2)
add_test(NAME RiderFloorDefaultHang COMMAND rider_floor_default_hang_test)
set_library_env(RiderFloorDefaultHang)

add_executable(rider_led_screen_object_test rider_led_screen_object_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_led_screen_object_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_led_screen_object_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderLedScreenObject COMMAND rider_led_screen_object_test)
set_library_env(RiderLedScreenObject)

add_executable(rider_hoist_import_test rider_hoist_import_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../core/dummyprofilelibrary.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_hoist_import_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_hoist_import_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderHoistImport COMMAND rider_hoist_import_test)
set_library_env(RiderHoistImport)

add_executable(rider_lx_sides_import_test rider_lx_sides_import_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_lx_sides_import_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_lx_sides_import_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderLxSidesImport COMMAND rider_lx_sides_import_test)
set_library_env(RiderLxSidesImport)

add_executable(rider_pipe_import_test rider_pipe_import_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_pipe_import_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_pipe_import_test PRIVATE ${wxWidgets_LIBRARIES}
                                                     tinyxml2::tinyxml2)
add_test(NAME RiderPipeImport COMMAND rider_pipe_import_test)
set_library_env(RiderPipeImport)

add_executable(rider_import_benchmark rider_import_benchmark.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/units/units.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/hoist_weight_distribution.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_benchmark PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_benchmark PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME RiderImportBenchmark COMMAND rider_import_benchmark
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_large.txt 1)
set_library_env(RiderImportBenchmark)


add_executable(fixture_symbol_projection_test fixture_symbol_projection_test.cpp
               ../viewer3d/render/opaque_pass_utils.cpp)
target_include_directories(fixture_symbol_projection_test PRIVATE
                           ../viewer3d ../viewer3d/render ../viewer2d ../models)
add_test(NAME FixtureSymbolProjection COMMAND fixture_symbol_projection_test)
add_bash_policy_test(FixtureSymbolProjectionParity ${CMAKE_SOURCE_DIR}/tests/check_fixture_symbol_projection_parity.sh)
set_tests_properties(FixtureSymbolProjectionParity PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(meshprimitives_cube_normals_test meshprimitives_cube_normals_test.cpp
               ../viewer3d/meshprimitives.cpp)
target_include_directories(meshprimitives_cube_normals_test PRIVATE ../viewer3d)
add_test(NAME MeshPrimitivesCubeNormals COMMAND meshprimitives_cube_normals_test)

add_executable(gdtfloader_primitive_test gdtfloader_primitive_test.cpp
               build_info_stub.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp
               ../core/logger.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_primitive_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_primitive_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME GdtfLoaderPrimitiveFallback COMMAND gdtfloader_primitive_test)
set_library_env(GdtfLoaderPrimitiveFallback)

add_executable(loader3ds_native_dimensions_test loader3ds_native_dimensions_test.cpp
               ../viewer3d/loader3ds.cpp
               consolepanel_stub.cpp)
target_include_directories(loader3ds_native_dimensions_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(loader3ds_native_dimensions_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME Loader3dsNativeDimensions COMMAND loader3ds_native_dimensions_test)
set_library_env(Loader3dsNativeDimensions)

add_executable(gdtfloader_channel_modes_test gdtfloader_channel_modes_test.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp
               ../core/logger.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               build_info_stub.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_channel_modes_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_channel_modes_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME GdtfLoaderChannelModeSummaries COMMAND gdtfloader_channel_modes_test)
set_library_env(GdtfLoaderChannelModeSummaries)

add_executable(gdtfloader_geometry_roots_test gdtfloader_geometry_roots_test.cpp
               build_info_stub.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp
               ../core/logger.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_geometry_roots_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_geometry_roots_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME GdtfLoaderGeometryRoots COMMAND gdtfloader_geometry_roots_test)
set_library_env(GdtfLoaderGeometryRoots)

add_executable(gdtfloader_set_properties_test gdtfloader_set_properties_test.cpp
               build_info_stub.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp
               ../core/logger.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_set_properties_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_set_properties_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME GdtfLoaderSetPropertiesMutation COMMAND gdtfloader_set_properties_test)
set_library_env(GdtfLoaderSetPropertiesMutation)

add_executable(symbol_fixture_applier_gdtf_test symbol_fixture_applier_gdtf_test.cpp
               build_info_stub.cpp
               ../gui/windows/symbol_fixture_applier.cpp
               ../gui/fixtures/fixture_gdtf_resolution.cpp
               ../gui/windows/symbol_preview_exporter.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/guiconfigservices.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/utf8_utils.cpp
               ../core/scene_grouping.cpp
               ../core/layer_service.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/uuidutils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/dummyprofilelibrary.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ../viewer3d/gdtfloader.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               consolepanel_stub.cpp
               ${LAYOUT_SOURCES})
target_include_directories(symbol_fixture_applier_gdtf_test PRIVATE
                           . .. ../core ../models ../gui ../viewer3d ../third_party)
target_link_libraries(symbol_fixture_applier_gdtf_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME SymbolFixtureApplierGdtfMutation COMMAND symbol_fixture_applier_gdtf_test)
set_library_env(SymbolFixtureApplierGdtfMutation)

add_executable(mvr_patched_gdtf_export_test mvr_patched_gdtf_export_test.cpp
               build_info_stub.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/utf8_utils.cpp
               ../core/scene_grouping.cpp
               ../core/layer_service.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/uuidutils.cpp
               ../core/dummyprofilelibrary.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ../viewer3d/gdtfloader.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/gdtf_catalog_matcher.cpp
               ../viewer2d/fixture_label_overrides.cpp
               consolepanel_stub.cpp
               ${LAYOUT_SOURCES})
target_include_directories(mvr_patched_gdtf_export_test PRIVATE
                           . .. ../core ../models ../mvr ../gui ../viewer3d ../third_party)
target_link_libraries(mvr_patched_gdtf_export_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2 perastage_test_gdtf_archive_reader)
add_test(NAME MvrPatchedGdtfExportMutation COMMAND mvr_patched_gdtf_export_test)
set_library_env(MvrPatchedGdtfExportMutation)


add_executable(user_preferences_store_test
               user_preferences_store_test.cpp
               ../core/configservices.cpp)
target_include_directories(user_preferences_store_test PRIVATE ../core ../third_party ../models)
target_link_libraries(user_preferences_store_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME UserPreferencesStore COMMAND user_preferences_store_test)

add_executable(project_session_test
               project_session_test.cpp
               ../core/configservices.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp)
target_include_directories(project_session_test PRIVATE ../core ../models ../third_party)
target_link_libraries(project_session_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME ProjectSession COMMAND project_session_test)


add_executable(filesystem_identity_test
               filesystem_identity_test.cpp
               build_info_stub.cpp
               ../core/filesystem_path_utils.cpp
               ../core/trussloader.cpp
               ../core/wx_path_utils.cpp
               ../core/runtime_storage.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/logger.cpp
               ../core/apppaths.cpp
               ../core/diagnostics/DiagnosticPaths.cpp)
target_include_directories(filesystem_identity_test PRIVATE ../core ../models ../third_party)
target_link_libraries(filesystem_identity_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME FilesystemIdentity COMMAND filesystem_identity_test)


add_executable(selection_state_test
               selection_state_test.cpp
               ../core/configservices.cpp)
target_include_directories(selection_state_test PRIVATE ../core ../third_party ../models)
target_link_libraries(selection_state_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME SelectionState COMMAND selection_state_test)

add_executable(history_manager_test
               history_manager_test.cpp
               ../core/configservices.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp)
target_include_directories(history_manager_test PRIVATE ../core ../models ../third_party)
target_link_libraries(history_manager_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME HistoryManager COMMAND history_manager_test)

add_executable(continuous_placement_scene_test
               continuous_placement_scene_test.cpp
               ../models/continuous_placement_scene.cpp)
target_include_directories(continuous_placement_scene_test PRIVATE
                           .. ../models)
add_test(NAME ContinuousPlacementScene COMMAND continuous_placement_scene_test)

add_executable(layer_visibility_state_test
               layer_visibility_state_test.cpp
               ../core/configservices.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp)
target_include_directories(layer_visibility_state_test PRIVATE ../core ../models ../third_party)
target_link_libraries(layer_visibility_state_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME LayerVisibilityState COMMAND layer_visibility_state_test)



add_executable(layout_image_resource_registry_test
               layout_image_resource_registry_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/projectutils.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../models/mvrscene.cpp
               mvr_io_stubs.cpp)
target_include_directories(layout_image_resource_registry_test PRIVATE
                           . ../core ../core/layouts ../models ../third_party)
target_link_libraries(layout_image_resource_registry_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME LayoutImageResourceRegistry COMMAND layout_image_resource_registry_test)
set_library_env(LayoutImageResourceRegistry)

add_executable(project_session_io_test
               project_session_io_test.cpp
               ../core/configservices.cpp)
target_include_directories(project_session_io_test PRIVATE ../core ../third_party ../models)
target_link_libraries(project_session_io_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME ProjectSessionIO COMMAND project_session_io_test)


add_executable(support_resolution_test
               support_resolution_test.cpp)
target_include_directories(support_resolution_test PRIVATE ../models)
add_test(NAME SupportResolution COMMAND support_resolution_test)

add_executable(symbol2d_image_builder_parallel_test
               symbol2d_image_builder_parallel_test.cpp
               ../core/symbols/Symbol2DImageBuilder.cpp)
target_include_directories(symbol2d_image_builder_parallel_test PRIVATE ../core)
add_test(NAME Symbol2DImageBuilderParallelExactEquality
         COMMAND symbol2d_image_builder_parallel_test)

add_executable(symbol_cache_manifest_test symbol_cache_manifest_test.cpp
               ../core/symbol_cache_manifest.cpp
               ../core/filesystem_path_utils.cpp)
target_include_directories(symbol_cache_manifest_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/third_party
    ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(symbol_cache_manifest_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME SymbolCacheManifestValidation COMMAND symbol_cache_manifest_test)
get_property(PERASTAGE_TEST_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(PERASTAGE_TEST_TARGET IN LISTS PERASTAGE_TEST_TARGETS)
    get_target_property(PERASTAGE_TEST_SOURCES ${PERASTAGE_TEST_TARGET} SOURCES)
    if(NOT PERASTAGE_TEST_TARGET STREQUAL "perastage_test_diagnostics" AND
       PERASTAGE_TEST_SOURCES AND
       ("../core/logger.cpp" IN_LIST PERASTAGE_TEST_SOURCES OR
        "../core/configservices.cpp" IN_LIST PERASTAGE_TEST_SOURCES))
        target_link_libraries(${PERASTAGE_TEST_TARGET} PRIVATE perastage_test_diagnostics)
    endif()
    if(NOT PERASTAGE_TEST_TARGET STREQUAL "perastage_test_path_utils" AND
       PERASTAGE_TEST_SOURCES AND
       ("../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp" IN_LIST PERASTAGE_TEST_SOURCES OR
        "../viewer3d/loader3ds.cpp" IN_LIST PERASTAGE_TEST_SOURCES OR
        "../viewer3d/loaderglb.cpp" IN_LIST PERASTAGE_TEST_SOURCES))
        target_link_libraries(${PERASTAGE_TEST_TARGET} PRIVATE perastage_test_path_utils)
    endif()
    if(NOT PERASTAGE_TEST_TARGET STREQUAL "perastage_test_gdtf_archive_reader" AND
       PERASTAGE_TEST_SOURCES AND
       "../viewer3d/gdtfloader.cpp
               ../core/runtime_storage.cpp" IN_LIST PERASTAGE_TEST_SOURCES AND
       NOT ("../core/gdtf_archive_reader.cpp" IN_LIST PERASTAGE_TEST_SOURCES))
        target_link_libraries(${PERASTAGE_TEST_TARGET} PRIVATE perastage_test_gdtf_archive_reader)
    endif()
endforeach()

add_executable(picking_coordinate_utils_test
               picking_coordinate_utils_test.cpp
               ../viewer3d/picking/picking_coordinate_utils.cpp)
target_include_directories(picking_coordinate_utils_test PRIVATE ../viewer3d/picking)
add_test(NAME PickingCoordinateUtils COMMAND picking_coordinate_utils_test)

add_executable(pick_mesh_validation_test
               pick_mesh_validation_test.cpp
               ../viewer3d/render/pick_mesh_validation.cpp)
target_include_directories(pick_mesh_validation_test PRIVATE
                           ../viewer3d
                           ../viewer3d/render)
add_test(NAME PickMeshValidation COMMAND pick_mesh_validation_test)

add_executable(mvr_xchange_protocol_test
    mvr_xchange_protocol_test.cpp
    ../mvr/xchange/mvr_xchange_commit.cpp
    ../mvr/xchange/mvr_xchange_dns_names.cpp
    ../mvr/xchange/mvr_xchange_message.cpp
    ../core/uuidutils.cpp
    ../mvr/xchange/mvr_xchange_network_interfaces.cpp
    ../mvr/xchange/mvr_xchange_station_registry.cpp
    ../mvr/xchange/mvr_xchange_packet.cpp
)
target_include_directories(mvr_xchange_protocol_test PRIVATE ${CMAKE_SOURCE_DIR}/mvr ${CMAKE_SOURCE_DIR}/third_party)
if(WIN32)
    target_link_libraries(mvr_xchange_protocol_test PRIVATE iphlpapi ws2_32)
endif()
add_test(NAME MvrXchangeProtocol COMMAND mvr_xchange_protocol_test)

add_executable(layer_service_utf8_test layer_service_utf8_test.cpp
               ../core/layer_service.cpp
               ../core/utf8_utils.cpp
               ../core/uuidutils.cpp
               ../models/layer.cpp
               configmanager_layoutmanager_stub.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp)
target_include_directories(layer_service_utf8_test PRIVATE ../core ../models)
target_link_libraries(layer_service_utf8_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME LayerServiceUtf8 COMMAND layer_service_utf8_test)


add_executable(dictionary_reset_transaction_test
               dictionary_reset_transaction_test.cpp
               mvr_io_stubs.cpp
               ../core/active_dictionary_storage.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/dictionary_reset_service.cpp
               ../core/file_import_utils.cpp
               ../core/filesystem_path_utils.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
               ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/layouts/LayoutTemplatePackageService.cpp
               ../core/projectutils.cpp
               ../core/runtime_storage.cpp
               ../core/library/library_bootstrap.cpp
               ../core/logger.cpp
               ../core/trussdictionary.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp)
target_include_directories(dictionary_reset_transaction_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(dictionary_reset_transaction_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME DictionaryResetTransaction COMMAND dictionary_reset_transaction_test)

add_executable(dictionary_duplicate_test
               dictionary_duplicate_test.cpp
               ../core/dictionary_duplicate.cpp
               ../core/active_dictionary_storage.cpp
               ../core/file_import_utils.cpp
               ../core/filesystem_path_utils.cpp)
target_include_directories(dictionary_duplicate_test PRIVATE ../core ../third_party)
add_test(NAME DictionaryDuplicate COMMAND dictionary_duplicate_test)

add_executable(active_dictionary_storage_test active_dictionary_storage_test.cpp)
target_include_directories(active_dictionary_storage_test PRIVATE ../core ../third_party)
target_link_libraries(active_dictionary_storage_test PRIVATE perastage_test_active_dictionary_storage)
add_test(NAME ActiveDictionaryStorage COMMAND active_dictionary_storage_test)

add_executable(gdtf_share_security_test
               gdtf_share_security_test.cpp
               ../core/gdtfnet.cpp
               ../core/credentialstore.cpp
               ../core/simplecrypt.cpp
               ../core/apppaths.cpp
               configmanager_layoutmanager_stub.cpp
               ../core/filesystem_path_utils.cpp
               ../core/logger.cpp
               ../core/diagnostics/DiagnosticLogger.cpp
               ../core/diagnostics/DiagnosticPaths.cpp)
target_include_directories(gdtf_share_security_test PRIVATE ../core ../third_party)
target_link_libraries(gdtf_share_security_test PRIVATE ${wxWidgets_LIBRARIES} CURL::libcurl)
add_test(NAME GdtfShareSecurity COMMAND gdtf_share_security_test)
set_tests_properties(GdtfShareSecurity PROPERTIES LABELS "gdtf-share;security;secure-store;release-gate")

add_executable(credential_store_native_roundtrip_test
               credential_store_native_roundtrip_test.cpp)
target_link_libraries(credential_store_native_roundtrip_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME CredentialStoreNativeRoundTrip COMMAND credential_store_native_roundtrip_test)
set_tests_properties(CredentialStoreNativeRoundTrip PROPERTIES
                     LABELS "gdtf-share;security;secure-store;release-gate"
                     SKIP_REGULAR_EXPRESSION "^SKIP:")

add_executable(stable_dataview_identity_index_test stable_dataview_identity_index_test.cpp
               ../gui/stable_dataview_identity_index.cpp)
target_include_directories(stable_dataview_identity_index_test PRIVATE ../gui)
add_test(NAME StableDataViewIdentityIndex COMMAND stable_dataview_identity_index_test)

add_executable(uuidutils_rfc4122_test uuidutils_rfc4122_test.cpp
               ../core/uuidutils.cpp)
target_include_directories(uuidutils_rfc4122_test PRIVATE ../core)
add_test(NAME UuidUtilsRfc4122 COMMAND uuidutils_rfc4122_test)
