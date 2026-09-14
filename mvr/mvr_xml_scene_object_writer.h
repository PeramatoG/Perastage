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

#include "types.h"

#include <optional>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
} // namespace tinyxml2

namespace mvr_xml_serialization {

struct FixtureValues {
  std::string uuid;
  std::string name;
  Matrix matrix;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string position;
  std::string fixtureId;
  int fixtureIdNumeric = 0;
  int unitNumber = 0;
  std::optional<int> absoluteDmxAddress;
  std::string color;
};

struct ObjectValues {
  std::string uuid;
  std::string name;
};

struct GeometryValues {
  std::string fileName;
  std::string geometryType;
  Matrix matrix;
};

struct SymbolValues {
  std::string uuid;
  std::string symdef;
  Matrix matrix;
};

struct GeometryReference {
  std::optional<GeometryValues> geometry;
  std::optional<SymbolValues> symbol;
};

struct TrussValues {
  ObjectValues object;
  Matrix matrix;
  std::string position;
  std::vector<GeometryReference> geometries;
  std::string function;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string fixtureId;
  int fixtureIdNumeric = 0;
  int unitNumber = 0;
  int customIdType = 0;
  int customId = 0;
};

struct SupportValues {
  ObjectValues object;
  Matrix matrix;
  std::string position;
  std::vector<GeometryValues> geometries;
  bool emitEmptyGeometries = false;
  std::string function;
  float chainLength = 0.0f;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string fixtureId;
  int fixtureIdNumeric = 0;
};

struct SceneObjectValues {
  ObjectValues object;
  Matrix matrix;
  std::vector<GeometryReference> geometries;
  std::string fixtureId;
  int fixtureIdNumeric = 0;
};

// Appends one standard MVR Fixture node using fully resolved export values.
void AppendFixture(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent, const FixtureValues &values);

// Appends one resolved standard MVR Truss node.
void AppendTruss(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *parent,
                 const TrussValues &values);

// Appends one resolved standard MVR Support node.
void AppendSupport(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent, const SupportValues &values);

// Appends one resolved standard MVR SceneObject node.
void AppendSceneObject(tinyxml2::XMLDocument &document,
                       tinyxml2::XMLElement *parent,
                       const SceneObjectValues &values);

// Creates the standard Layers container.
tinyxml2::XMLElement *CreateLayers(tinyxml2::XMLDocument &document);

// Appends a standard Layer containing an already-populated ChildList.
void AppendLayer(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *layers,
                 const ObjectValues &values, tinyxml2::XMLElement *childList);

// Creates a standard GroupObject with matrix and ChildList.
void AppendGroupObject(tinyxml2::XMLDocument &document,
                       tinyxml2::XMLElement *parent, const ObjectValues &values,
                       const Matrix &matrix, tinyxml2::XMLElement *childList);

// Creates an unattached standard ChildList.
tinyxml2::XMLElement *CreateChildList(tinyxml2::XMLDocument &document);

} // namespace mvr_xml_serialization
