/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
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
