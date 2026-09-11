#pragma once

#include "mvr_scene_node_reader.h"

namespace mvr::scene_reader_detail {

std::string Trim(const std::string &value);
std::string ToLowerCopy(std::string value);
bool TryParseFloat(const std::string &text, float &out);

std::string DescribeTruss(const Truss &truss);
Truss::GeometryRepresentation
ParseTrussRepresentation(const std::string &value);
bool IsRenderableTrussGeometry(const std::string &path);
void ApplySupportDefaults(Support &support);
void ReadFixtureCategory(tinyxml2::XMLElement *fixtureNode, Fixture &fixture);
SceneReadLegacyFixtureIdentity
ReadLegacyFixtureIdentity(tinyxml2::XMLElement *fixtureNode);
void ReadSupportHoistInfo(tinyxml2::XMLElement *info, Support &support,
                          std::vector<MvrImportDiagnostic> &diagnostics);
void ReadSupportHoistUserData(tinyxml2::XMLElement *supportNode,
                              Support &support,
                              std::vector<MvrImportDiagnostic> &diagnostics);

} // namespace mvr::scene_reader_detail
