/* This file is part of Perastage. */
#pragma once

#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <string>
#include <vector>

namespace tinyxml2 { class XMLElement; }

namespace mvr {
// Imports the scene-wide class registry and records unrecoverable source facts.
void ReadMvrClasses(tinyxml2::XMLElement &root, MvrScene &scene);

// Returns a semantic UUID; malformed source values remain protected by evidence.
std::string ReadMvrClassing(tinyxml2::XMLElement &object);

// Validates class ownership and references before identity recovery can run.
std::vector<MvrExportDiagnostic> CheckMvrClasses(const MvrScene &scene);

// Confirms serialization retained the complete class registry and assignments.
std::vector<MvrExportDiagnostic> CheckSerializedClasses(
    tinyxml2::XMLElement &sceneElement, const MvrScene &scene);
} // namespace mvr
