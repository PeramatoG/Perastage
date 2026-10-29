#pragma once

#include "mvr_identifier_source.h"
#include "mvr_export_diagnostic.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class MvrScene;
namespace tinyxml2 { class XMLElement; }

namespace mvr_identifiers {
bool ParseInteger(const std::string &text, int &value);
std::optional<int> ReadOptionalInteger(const tinyxml2::XMLElement &node, const char *name);
void Read(const tinyxml2::XMLElement &node, std::string &text, int &numeric,
          int &unit, std::optional<MvrIdentifierSource> &source);
template <typename Object>
void Read(const tinyxml2::XMLElement &node, Object &object) {
  Read(node, object.fixtureIdText, object.fixtureIdNumeric, object.unitNumber,
       object.identifierSource);
}
std::optional<int> OptionalUnit(const std::optional<MvrIdentifierSource> &source,
                                int value);
void ScanSource(const tinyxml2::XMLElement &node, MvrScene &scene,
                const std::string &location);
bool ValidateElement(const tinyxml2::XMLElement &node, bool requiredUnit);
std::vector<MvrExportDiagnostic> CheckProjection(
    const tinyxml2::XMLElement &root, const MvrScene &scene,
    const std::unordered_map<std::string, std::pair<std::string, int>> &ids,
    const std::unordered_map<std::string, int> &units);
} // namespace mvr_identifiers
