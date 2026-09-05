target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_catalog_matcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtf_catalog_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrimporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_applier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvr_merge_resource_rewriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrscenemerger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvrexporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/primitive_model_resources.cpp
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
