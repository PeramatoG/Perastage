add_library(perastage_mvr_read STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_catalog_matcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_package.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_reference_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_resource_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_read_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_scene_node_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_scene_node_reader_support.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/primitive_model_resources.cpp
)
target_compile_features(perastage_mvr_read PUBLIC cxx_std_20)
target_include_directories(perastage_mvr_read
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/models
    PRIVATE ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/viewer3d
            ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(perastage_mvr_read
    PUBLIC perastage_uuid perastage_runtime_storage
    PRIVATE perastage_mvr_core_support perastage_mvr_scene_model
            tinyxml2::tinyxml2 ${_wx_base_libs}
)

target_link_libraries(${PROJECT_NAME} PRIVATE perastage_mvr_read)

add_library(perastage_mvr_import_application_read STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_application_read_services.cpp
)
target_compile_features(perastage_mvr_import_application_read PUBLIC cxx_std_20)
target_include_directories(perastage_mvr_import_application_read
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/models
    PRIVATE ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/viewer3d
)
target_link_libraries(perastage_mvr_import_application_read
    PUBLIC perastage_mvr_read
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    perastage_mvr_import_application_read
)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_catalog_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrimporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_application_read_services.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_project_application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_project_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_resource_resolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_reference_resolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_import_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_read_service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_scene_node_reader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_scene_node_reader_detail.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_applier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_resource_rewriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrscenemerger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_export_archive_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_export_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_export_resource_collection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_export_gdtf_resources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_xml_document_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_xml_extension_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_xml_scene_object_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_export_preparation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_commit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_dns_names.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_mdns_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_mdns_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_mdns_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_network_interfaces.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_remote_station.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_station_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_tcp_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_message.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_packet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_publication_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xchange/mvr_xchange_tcp_server.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 iphlpapi)
endif()

if(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PERASTAGE_MVR_XCHANGE_ENABLE_MDNS=1
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE mdns::mdns)
endif()
