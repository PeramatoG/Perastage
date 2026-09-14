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

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

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

// Appends one standard MVR Fixture node using fully resolved export values.
void AppendFixture(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent,
                   const FixtureValues &values);

// Creates a named standard MVR object node with resolved identity values.
tinyxml2::XMLElement *CreateObject(tinyxml2::XMLDocument &document,
                                   const char *nodeName,
                                   const ObjectValues &values);

// Creates a standard MVR hierarchy container node.
tinyxml2::XMLElement *CreateContainer(tinyxml2::XMLDocument &document,
                                      const char *nodeName);

// Appends a standard text child when the resolved value is non-empty.
void AppendText(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *parent,
                const char *nodeName, const std::string &value);

// Appends a standard integer child when the resolved value is nonzero.
void AppendInteger(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent, const char *nodeName,
                   int value);

// Appends a resolved standard Geometry3D node and matrix.
void AppendGeometry(tinyxml2::XMLDocument &document,
                    tinyxml2::XMLElement *geometries,
                    const GeometryValues &values);

// Appends a resolved standard Symbol node and matrix.
void AppendSymbol(tinyxml2::XMLDocument &document,
                  tinyxml2::XMLElement *geometries,
                  const SymbolValues &values);

} // namespace mvr_xml_serialization
