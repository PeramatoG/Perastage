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
#include "mvr_xml_scene_object_writer.h"

#include "matrixutils.h"

#include <tinyxml2.h>

namespace mvr_xml_serialization {

// Appends one standard MVR Fixture node in the established child order.
void AppendFixture(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent,
                   const FixtureValues &values) {
  tinyxml2::XMLElement *fixture = document.NewElement("Fixture");
  fixture->SetAttribute("uuid", values.uuid.c_str());
  fixture->SetAttribute("name", values.name.c_str());

  auto addInt = [&](const char *name, int value) {
    if (value != 0) {
      tinyxml2::XMLElement *element = document.NewElement(name);
      element->SetText(std::to_string(value).c_str());
      fixture->InsertEndChild(element);
    }
  };
  auto addString = [&](const char *name, const std::string &value) {
    if (!value.empty()) {
      tinyxml2::XMLElement *element = document.NewElement(name);
      element->SetText(value.c_str());
      fixture->InsertEndChild(element);
    }
  };

  tinyxml2::XMLElement *matrix = document.NewElement("Matrix");
  matrix->SetText(MatrixUtils::FormatMatrix(values.matrix).c_str());
  fixture->InsertEndChild(matrix);
  addString("GDTFSpec", values.gdtfSpec);
  addString("GDTFMode", values.gdtfMode);
  addString("Position", values.position);
  addString("FixtureID", values.fixtureId);
  addInt("FixtureIDNumeric", values.fixtureIdNumeric);
  addInt("UnitNumber", values.unitNumber);
  if (values.absoluteDmxAddress.has_value()) {
    tinyxml2::XMLElement *addresses = document.NewElement("Addresses");
    tinyxml2::XMLElement *address = document.NewElement("Address");
    address->SetAttribute("break", 0);
    address->SetText(std::to_string(*values.absoluteDmxAddress).c_str());
    addresses->InsertEndChild(address);
    fixture->InsertEndChild(addresses);
  }
  addString("Color", values.color);
  parent->InsertEndChild(fixture);
}

// Creates a named standard MVR object node with resolved identity values.
tinyxml2::XMLElement *CreateObject(tinyxml2::XMLDocument &document,
                                   const char *nodeName,
                                   const ObjectValues &values) {
  tinyxml2::XMLElement *object = document.NewElement(nodeName);
  object->SetAttribute("uuid", values.uuid.c_str());
  if (!values.name.empty())
    object->SetAttribute("name", values.name.c_str());
  return object;
}

// Creates a standard MVR hierarchy container node.
tinyxml2::XMLElement *CreateContainer(tinyxml2::XMLDocument &document,
                                      const char *nodeName) {
  return document.NewElement(nodeName);
}

// Appends a standard text child when the resolved value is non-empty.
void AppendText(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *parent,
                const char *nodeName, const std::string &value) {
  if (value.empty())
    return;
  tinyxml2::XMLElement *element = document.NewElement(nodeName);
  element->SetText(value.c_str());
  parent->InsertEndChild(element);
}

// Appends a standard integer child when the resolved value is nonzero.
void AppendInteger(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent, const char *nodeName,
                   int value) {
  if (value == 0)
    return;
  tinyxml2::XMLElement *element = document.NewElement(nodeName);
  element->SetText(std::to_string(value).c_str());
  parent->InsertEndChild(element);
}

// Appends a resolved standard Geometry3D node and matrix.
void AppendGeometry(tinyxml2::XMLDocument &document,
                    tinyxml2::XMLElement *geometries,
                    const GeometryValues &values) {
  tinyxml2::XMLElement *geometry = document.NewElement("Geometry3D");
  geometry->SetAttribute("fileName", values.fileName.c_str());
  if (!values.geometryType.empty())
    geometry->SetAttribute("geometryType", values.geometryType.c_str());
  tinyxml2::XMLElement *matrix = document.NewElement("Matrix");
  matrix->SetText(MatrixUtils::FormatMatrix(values.matrix).c_str());
  geometry->InsertEndChild(matrix);
  geometries->InsertEndChild(geometry);
}

// Appends a resolved standard Symbol node and matrix.
void AppendSymbol(tinyxml2::XMLDocument &document,
                  tinyxml2::XMLElement *geometries,
                  const SymbolValues &values) {
  tinyxml2::XMLElement *symbol = document.NewElement("Symbol");
  symbol->SetAttribute("uuid", values.uuid.c_str());
  symbol->SetAttribute("symdef", values.symdef.c_str());
  tinyxml2::XMLElement *matrix = document.NewElement("Matrix");
  matrix->SetText(MatrixUtils::FormatMatrix(values.matrix).c_str());
  symbol->InsertEndChild(matrix);
  geometries->InsertEndChild(symbol);
}

} // namespace mvr_xml_serialization
