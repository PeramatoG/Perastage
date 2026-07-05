cmake_minimum_required(VERSION 3.21)
project(PerastageTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)

enable_testing()

add_library(perastage_test_diagnostics STATIC
            ../core/apppaths.cpp
            ../core/configservices.cpp
            ../core/diagnostics/DiagnosticPaths.cpp
            ../core/filesystem_path_utils.cpp
            ../core/symbol_cache_manifest.cpp)
target_include_directories(perastage_test_diagnostics PRIVATE
                           ../core
                           ../third_party)

add_library(perastage_test_path_utils STATIC
            ../core/filesystem_path_utils.cpp)
target_include_directories(perastage_test_path_utils PRIVATE ../core)

find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
    add_test(NAME DocsPerastageTreeModules
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_perastage_tree_modules.sh)
    add_test(NAME GuiNoConfigManagerGet
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_no_configmanager_get_in_gui.sh)
    add_test(NAME OpenGLLifecycleCentralized
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_opengl_lifecycle_centralized.sh)
    add_test(NAME Layout2DRasterizationBoundary
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_layout_2d_rasterization_boundary.sh)
    add_test(NAME LibraryUserDataPolicy
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_library_user_data_policy.sh)
    add_test(NAME DictionaryPathsPolicy
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_dictionary_paths.sh)
    add_test(NAME ProjectRestoreImportOptions
             COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_project_restore_import_options.sh)
endif()

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


add_executable(platform_locale_test
               platform_locale_test.cpp
               ../core/platform_locale.cpp)
target_include_directories(platform_locale_test PRIVATE ../core)
add_test(NAME PlatformLocaleSetup COMMAND platform_locale_test)

add_executable(trussloader_validation_test
               trussloader_validation_test.cpp
               ../core/trussloader.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/logger.cpp
               ../models/truss.cpp)
target_include_directories(trussloader_validation_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(trussloader_validation_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME TrussLoaderValidation COMMAND trussloader_validation_test)

add_executable(truss_creation_source_test
               truss_creation_source_test.cpp
               ../gui/truss_creation_source.cpp
               ../core/trussloader.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/logger.cpp
               ../core/filesystem_path_utils.cpp
               ../models/truss.cpp)
target_include_directories(truss_creation_source_test PRIVATE
                           . ../core ../gui ../models ../third_party)
target_link_libraries(truss_creation_source_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME TrussCreationSource COMMAND truss_creation_source_test)


add_executable(mvr_merge_analyzer_applier_test
               mvr_merge_analyzer_applier_test.cpp
               ../mvr/mvr_merge_analyzer.cpp
               ../mvr/mvr_merge_applier.cpp
               ../mvr/mvr_merge_resource_rewriter.cpp
               ../mvr/mvrscenemerger.cpp
               ../core/uuidutils.cpp
               ../models/mvrscene.cpp)
add_test(NAME MvrMergeAnalyzerApplier COMMAND mvr_merge_analyzer_applier_test)

add_executable(matrixutils_test matrixutils_test.cpp)
target_include_directories(matrixutils_test PRIVATE ../models)
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
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
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
target_include_directories(console_command_parser_test PRIVATE .. ../gui)
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
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp)
target_include_directories(gdtf_canonicalizer_test PRIVATE ../core ../third_party)
target_link_libraries(gdtf_canonicalizer_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfCanonicalizerExportRules COMMAND gdtf_canonicalizer_test)

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
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
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

add_executable(gdtf_dictionary_color_mode_propagation_test
               gdtf_dictionary_color_mode_propagation_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/gdtfdictionary.cpp
               ../core/layouts/LayoutManager.cpp
    ../core/layouts/LayoutImageResourceRegistry.cpp
               ../core/layouts/LayoutDefaultsLoader.cpp
               ../core/layouts/LayoutCollection.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/projectutils.cpp
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
               ../core/layouts/LayoutTemplateSerializer.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/logger.cpp
               ../models/truss.cpp
               ../models/mvrscene.cpp)
target_include_directories(dictionary_seed_merge_backup_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(dictionary_seed_merge_backup_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME DictionarySeedMergeBackup COMMAND dictionary_seed_merge_backup_test)

add_executable(layout_template_serializer_test
               layout_template_serializer_test.cpp
               ../core/layouts/LayoutTemplateSerializer.cpp)
add_test(NAME LayoutTemplateSerializer COMMAND layout_template_serializer_test)

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
               ../core/layouts/LayoutTemplateSerializer.cpp)
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
               ../core/layouts/LayoutTemplateSerializer.cpp)
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
    ../core/layouts/LayoutTemplateSerializer.cpp)

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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               configmanager_layoutmanager_stub.cpp
               ../core/projectutils.cpp
               ../core/apppaths.cpp
               ../core/filesystem_path_utils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
               ../core/truss_gdtf_builder.cpp
               ../core/logger.cpp
               ../models/truss.cpp)
target_include_directories(truss_path_encoding_regression_test PRIVATE
                           . ../core ../models ../third_party)
target_link_libraries(truss_path_encoding_regression_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME TrussPathEncodingRegression COMMAND truss_path_encoding_regression_test)

add_executable(rider_truss_dictionary_normalization_test
               rider_truss_dictionary_normalization_test.cpp
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
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
set_library_env(MvrTrussRoundtripStructure)

add_executable(mvr_exporter_compliance_test mvr_exporter_compliance_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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


add_executable(meshprimitives_cube_normals_test meshprimitives_cube_normals_test.cpp
               ../viewer3d/meshprimitives.cpp)
target_include_directories(meshprimitives_cube_normals_test PRIVATE ../viewer3d)
add_test(NAME MeshPrimitivesCubeNormals COMMAND meshprimitives_cube_normals_test)

add_executable(gdtfloader_primitive_test gdtfloader_primitive_test.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_primitive_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_primitive_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
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
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_channel_modes_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_channel_modes_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfLoaderChannelModeSummaries COMMAND gdtfloader_channel_modes_test)
set_library_env(GdtfLoaderChannelModeSummaries)

add_executable(gdtfloader_geometry_roots_test gdtfloader_geometry_roots_test.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_geometry_roots_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_geometry_roots_test PRIVATE
                      ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfLoaderGeometryRoots COMMAND gdtfloader_geometry_roots_test)
set_library_env(GdtfLoaderGeometryRoots)

add_executable(gdtfloader_set_properties_test gdtfloader_set_properties_test.cpp
               ../viewer3d/gdtfloader.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_set_properties_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_set_properties_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfLoaderSetPropertiesMutation COMMAND gdtfloader_set_properties_test)
set_library_env(GdtfLoaderSetPropertiesMutation)

add_executable(symbol_fixture_applier_gdtf_test symbol_fixture_applier_gdtf_test.cpp
               ../gui/windows/symbol_fixture_applier.cpp
               ../gui/fixtures/fixture_gdtf_resolution.cpp
               ../gui/windows/symbol_preview_exporter.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/guiconfigservices.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/uuidutils.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
target_link_libraries(symbol_fixture_applier_gdtf_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME SymbolFixtureApplierGdtfMutation COMMAND symbol_fixture_applier_gdtf_test)
set_library_env(SymbolFixtureApplierGdtfMutation)

add_executable(mvr_patched_gdtf_export_test mvr_patched_gdtf_export_test.cpp
               ../mvr/mvrexporter.cpp
               ../mvr/primitive_model_resources.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/library/library_bootstrap.cpp
               ../core/gdtfdictionary.cpp
               ../core/gdtf_fixture_category.cpp
               ../core/gdtf_canonicalizer.cpp
               ../core/gdtf_mutation_audit.cpp
               ../core/trussdictionary.cpp
               ../core/trussloader.cpp
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
target_link_libraries(mvr_patched_gdtf_export_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
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
               ../core/projectutils.cpp
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
       ("../viewer3d/gdtfloader.cpp" IN_LIST PERASTAGE_TEST_SOURCES OR
        "../viewer3d/loader3ds.cpp" IN_LIST PERASTAGE_TEST_SOURCES OR
        "../viewer3d/loaderglb.cpp" IN_LIST PERASTAGE_TEST_SOURCES))
        target_link_libraries(${PERASTAGE_TEST_TARGET} PRIVATE perastage_test_path_utils)
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
