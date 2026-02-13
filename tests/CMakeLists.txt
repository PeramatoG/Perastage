cmake_minimum_required(VERSION 3.21)
project(PerastageTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base aui gl html)
include(${wxWidgets_USE_FILE})
find_package(tinyxml2 CONFIG REQUIRED)

enable_testing()

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

add_executable(matrixutils_test matrixutils_test.cpp)
target_include_directories(matrixutils_test PRIVATE ../models)
add_test(NAME MatrixUtilsParsingAndComposition COMMAND matrixutils_test)

add_executable(pdf_font_metrics_test
               pdf_font_metrics_test.cpp
               ../viewer2d/pdf/font_metrics.cpp)
add_test(NAME PdfFontMetricsEncoding COMMAND pdf_font_metrics_test)

add_executable(pdf_writer_test
               pdf_writer_test.cpp
               ../viewer2d/pdf/pdf_writer.cpp)
add_test(NAME PdfWriterSerialization COMMAND pdf_writer_test)

add_executable(fixture_table_parser_test
               fixture_table_parser_test.cpp
               ../gui/fixturetable/fixture_table_parser.cpp)
target_include_directories(fixture_table_parser_test PRIVATE .. ../gui)
target_link_libraries(fixture_table_parser_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME FixtureTableParser COMMAND fixture_table_parser_test)

function(set_library_env TEST_NAME)
    set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "PERASTAGE_LIBRARY_PATH=${CMAKE_SOURCE_DIR}/library")
endfunction()

set(LAYOUT_SOURCES
    ../core/layouts/LayoutManager.cpp
    ../core/layouts/LayoutCollection.cpp)

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
               ../core/gdtfdictionary.cpp
               ../core/trussdictionary.cpp
               ../core/uuidutils.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/mvrexporter.cpp
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

add_executable(mvr_dmx_address_test mvr_dmx_address_test.cpp
               ../mvr/mvrexporter.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/gdtfdictionary.cpp
               ../core/trussdictionary.cpp
               ../core/uuidutils.cpp
               ../mvr/mvrimporter.cpp
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

add_executable(mvr_exporter_compliance_test mvr_exporter_compliance_test.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/gdtfdictionary.cpp
               ../core/trussdictionary.cpp
               ../core/uuidutils.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/mvrexporter.cpp
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
               trussloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../core/gdtfdictionary.cpp
               ../core/trussdictionary.cpp
               ../mvr/mvrimporter.cpp
               ../mvr/mvrexporter.cpp
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
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(riderimporter_fixtureid_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(riderimporter_fixtureid_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME RiderImporterFixtureIds COMMAND riderimporter_fixtureid_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_basic.txt
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_over100.txt)
set_library_env(RiderImporterFixtureIds)

add_executable(rider_import_order_test rider_import_order_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_import_order_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_import_order_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME RiderImportOrder COMMAND rider_import_order_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixture_order.txt)
set_library_env(RiderImportOrder)

add_executable(rider_autopatch_test rider_autopatch_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_onech_stub.cpp
               ../core/riderimporter.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_autopatch_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_autopatch_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME RiderAutoPatch COMMAND rider_autopatch_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_fixtureid_basic.txt)
set_library_env(RiderAutoPatch)

add_executable(rider_layer_mode_test rider_layer_mode_test.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
               ../models/mvrscene.cpp
               ../models/fixture.cpp
               ../models/truss.cpp
               ../models/sceneobject.cpp
               ../models/layer.cpp
               ${LAYOUT_SOURCES})
target_include_directories(rider_layer_mode_test PRIVATE
                           . ../core ../models ../mvr ../gui ../third_party)
target_link_libraries(rider_layer_mode_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME RiderLayerMode COMMAND rider_layer_mode_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/rider_layer_mode.txt)
set_library_env(RiderLayerMode)

add_executable(rider_import_benchmark rider_import_benchmark.cpp
               riderimporter_stubs.cpp
               gdtfloader_stub.cpp
               ../core/riderimporter.cpp
               ../core/uuidutils.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp
               ../core/configmanager.cpp
               ../core/configservices.cpp
               ../core/projectutils.cpp
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


add_executable(gdtfloader_primitive_test gdtfloader_primitive_test.cpp
               ../viewer3d/gdtfloader.cpp
               ../viewer3d/loader3ds.cpp
               ../viewer3d/loaderglb.cpp
               ../viewer3d/meshprimitives.cpp
               consolepanel_stub.cpp)
target_include_directories(gdtfloader_primitive_test PRIVATE
                           . ../viewer3d ../models ../gui ../third_party)
target_link_libraries(gdtfloader_primitive_test PRIVATE ${wxWidgets_LIBRARIES} tinyxml2::tinyxml2)
add_test(NAME GdtfLoaderPrimitiveFallback COMMAND gdtfloader_primitive_test)
set_library_env(GdtfLoaderPrimitiveFallback)


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


add_executable(project_session_io_test
               project_session_io_test.cpp
               ../core/configservices.cpp)
target_include_directories(project_session_io_test PRIVATE ../core ../third_party ../models)
target_link_libraries(project_session_io_test PRIVATE ${wxWidgets_LIBRARIES})
add_test(NAME ProjectSessionIO COMMAND project_session_io_test)
