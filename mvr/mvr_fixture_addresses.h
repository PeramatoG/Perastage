/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
namespace tinyxml2 { class XMLElement; }
namespace mvr {
// Preserves only the standard Fixture Address subset; records unknown content.
void ReadFixtureAddresses(const tinyxml2::XMLElement &node, Fixture &fixture, MvrScene &scene);
std::vector<MvrExportDiagnostic> CheckFixtureAddressProjection(
    const tinyxml2::XMLElement &root, const MvrScene &scene);
void ScanFixtureAddresses(const tinyxml2::XMLElement &node, MvrScene &scene);
}
