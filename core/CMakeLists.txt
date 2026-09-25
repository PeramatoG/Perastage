add_library(perastage_mvr_core_support STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture_visual_color.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_fixture_category.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene_grouping.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.cpp
)
target_compile_features(perastage_mvr_core_support PUBLIC cxx_std_20)
target_include_directories(perastage_mvr_core_support
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/models
)
target_link_libraries(perastage_mvr_core_support
    PUBLIC perastage_uuid
    PRIVATE tinyxml2::tinyxml2 ${_wx_base_libs}
)

add_library(perastage_inspection_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/inspection_contract.cpp
)

file(READ "${CMAKE_SOURCE_DIR}/resources/standards/gdtf/1.2/gdtf.xsd"
     PERASTAGE_GDTF_12_XSD)
file(READ "${CMAKE_SOURCE_DIR}/resources/standards/mvr/1.6/mvr.xsd"
     PERASTAGE_MVR_16_XSD)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/standard_schemas.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/standard_schemas.h
    @ONLY
)
add_library(perastage_inspection_validation STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/xml_schema_validation.cpp
)
target_compile_features(perastage_inspection_validation PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_validation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(perastage_inspection_validation
    PUBLIC perastage_inspection_core
    PRIVATE LibXml2::LibXml2
)

target_compile_features(perastage_inspection_core PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(perastage_inspection_serialization STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/inspection_json_serializer.cpp
)

target_compile_features(perastage_inspection_serialization PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_serialization
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(perastage_inspection_serialization
    PUBLIC perastage_inspection_core
)

add_library(perastage_archive_zip_directory STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/archive_zip_directory.cpp
)

add_library(perastage_runtime_storage STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/DiagnosticPaths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_path_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_storage.cpp
)
target_compile_features(perastage_runtime_storage PUBLIC cxx_std_20)
target_include_directories(perastage_runtime_storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics
)

target_compile_features(perastage_archive_zip_directory PUBLIC cxx_std_20)
target_include_directories(perastage_archive_zip_directory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(perastage_inspection_package STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/package_inspection.cpp
)

add_library(perastage_inspection_resource STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/resource_inspection.cpp
)

add_library(perastage_gdtf_read STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_archive_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_description_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/gdtf_document.cpp
)

target_compile_features(perastage_gdtf_read PUBLIC cxx_std_20)
target_include_directories(perastage_gdtf_read PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(perastage_gdtf_read
    PUBLIC perastage_archive_zip_directory
    PRIVATE perastage_runtime_storage tinyxml2::tinyxml2 ${_wx_base_libs}
)

add_library(perastage_inspection_gdtf STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/gdtf_inspection.cpp
)

add_library(perastage_inspection_mvr STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/mvr_inspection.cpp
)

target_compile_features(perastage_inspection_mvr PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_mvr
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/models ${CMAKE_SOURCE_DIR}/mvr
)
target_link_libraries(perastage_inspection_mvr
    PUBLIC perastage_inspection_core perastage_inspection_package
           perastage_inspection_validation
           perastage_mvr_read
)

target_compile_features(perastage_inspection_gdtf PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_gdtf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(perastage_inspection_gdtf
    PUBLIC perastage_inspection_core perastage_inspection_package
           perastage_inspection_validation
           perastage_gdtf_read
)

target_compile_features(perastage_inspection_resource PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_resource PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(perastage_inspection_resource
    PUBLIC perastage_inspection_core perastage_inspection_package
    PRIVATE perastage_archive_zip_directory perastage_gdtf_read ${_wx_base_libs}
)

add_library(perastage_inspection_nested_gdtf STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inspection/nested_gdtf_inspection.cpp
)
target_compile_features(perastage_inspection_nested_gdtf PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_nested_gdtf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(perastage_inspection_nested_gdtf
    PUBLIC perastage_inspection_resource perastage_inspection_gdtf
)

target_compile_features(perastage_inspection_package PUBLIC cxx_std_20)
target_include_directories(perastage_inspection_package PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(perastage_inspection_package
    PUBLIC perastage_inspection_core
    PRIVATE perastage_archive_zip_directory
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    perastage_gdtf_read
    perastage_runtime_storage
)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry_bounds_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layer_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_dimension_resolution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/active_dictionary_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/autopatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/apppaths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture_line_distribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configservices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/credentialstore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/CrashHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/DiagnosticLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/DiagnosticReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary_bundle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary_duplicate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary_reset_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary_snapshot_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dummyprofilelibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_import_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture_gdtf_derivative_contract.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fixture_gdtf_derivative_publication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtfdictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_fixture_insertion_preparation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_metadata_summary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/gdtf_color_cie.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/gdtf_wheel_catalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/gdtf_dmx_inspector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/gdtf_mode_channel_browser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_catalog_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_download_workflow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_download_filename.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_canonicalizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_document_mutation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_mutation_audit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/gdtf_edit_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/gdtf_editable_values.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/gdtf_editor_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/gdtf_field_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/project_fixture_gdtf_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/project_truss_gdtf_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/project_truss_gdtf_apply_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf/editor/standalone_gdtf_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtfnet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/RdpSimplifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/guiconfigservices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hoistpresetlibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hoist_weight_distribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/library_bootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/localization/app_language.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/localization/localization_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutCollection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutDefaultsLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutImageResourceRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutTemplateSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutTemplatePackageService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/LayoutSelectionPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layoutviewpresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/markdown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/magnet_snap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_identity_recovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_attachment_paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/patchmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_locale.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdftext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/PageSetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/Viewer2DPrintSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/projectutils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project_fixture_gdtf_consolidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/startup_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rigging_extra_weight_settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/riderimporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rider_fixture_resolution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rider_text_semantics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene_clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene_node_operations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene_transform_integrity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene_object_truss_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scenedatamanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simplecrypt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/Symbol2DImageBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/FixtureSymbolDiagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/fixture_symbol_availability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/fixture_symbol_preparation_coordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/fixture_symbol_preparation_requests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/fixture_symbol_resource_revision.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/fixture_symbol_svg_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/SymbolGeometrySimplifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/Symbol2DBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols/PerastageSvgSymbol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/trussdictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_asset_ingestion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_attachment_candidates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_screen_snap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/truss_gdtf_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/trussloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui_render_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pixel_coordinate_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui_unit_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wx_path_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/units/unit_label_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/units/units.cpp
)

target_sources(perastage_uuid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/uuidutils.cpp
)

target_include_directories(perastage_uuid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts
    ${CMAKE_CURRENT_SOURCE_DIR}/print
)
