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
                   tinyxml2::XMLElement *parent, const FixtureValues &values) {
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

namespace {

// Creates a standard object element with resolved identity attributes.
tinyxml2::XMLElement *CreateObject(tinyxml2::XMLDocument &document,
                                   const char *nodeName,
                                   const ObjectValues &values) {
  tinyxml2::XMLElement *object = document.NewElement(nodeName);
  if (!values.uuid.empty())
    object->SetAttribute("uuid", values.uuid.c_str());
  if (!values.name.empty())
    object->SetAttribute("name", values.name.c_str());
  return object;
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

// Appends all resolved geometry references beneath a Geometries node.
void AppendGeometryReferences(tinyxml2::XMLDocument &document,
                              tinyxml2::XMLElement *parent,
                              const std::vector<GeometryReference> &values,
                              bool emitEmpty) {
  if (values.empty() && !emitEmpty)
    return;
  tinyxml2::XMLElement *geometries = document.NewElement("Geometries");
  for (const GeometryReference &value : values) {
    if (value.geometry)
      AppendGeometry(document, geometries, *value.geometry);
    else if (value.symbol)
      AppendSymbol(document, geometries, *value.symbol);
  }
  parent->InsertEndChild(geometries);
}

} // namespace

// Appends one resolved standard MVR Truss node in established child order.
void AppendTruss(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *parent,
                 const TrussValues &values) {
  tinyxml2::XMLElement *truss = CreateObject(document, "Truss", values.object);
  AppendText(document, truss, "Matrix",
             MatrixUtils::FormatMatrix(values.matrix));
  AppendText(document, truss, "Position", values.position);
  AppendGeometryReferences(document, truss, values.geometries, false);
  AppendText(document, truss, "Function", values.function);
  AppendText(document, truss, "GDTFSpec", values.gdtfSpec);
  AppendText(document, truss, "GDTFMode", values.gdtfMode);
  AppendText(document, truss, "FixtureID", values.fixtureId);
  AppendInteger(document, truss, "FixtureIDNumeric", values.fixtureIdNumeric);
  AppendInteger(document, truss, "UnitNumber", values.unitNumber);
  AppendInteger(document, truss, "CustomIdType", values.customIdType);
  AppendInteger(document, truss, "CustomId", values.customId);
  parent->InsertEndChild(truss);
}

// Appends one resolved standard MVR Support node in established child order.
void AppendSupport(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent, const SupportValues &values) {
  tinyxml2::XMLElement *support =
      CreateObject(document, "Support", values.object);
  AppendText(document, support, "Matrix",
             MatrixUtils::FormatMatrix(values.matrix));
  AppendText(document, support, "Position", values.position);
  std::vector<GeometryReference> references;
  for (const GeometryValues &geometry : values.geometries)
    references.push_back({geometry, std::nullopt});
  AppendGeometryReferences(document, support, references,
                           values.emitEmptyGeometries);
  AppendText(document, support, "Function", values.function);
  AppendText(document, support, "ChainLength",
             std::to_string(values.chainLength));
  AppendText(document, support, "GDTFSpec", values.gdtfSpec);
  AppendText(document, support, "GDTFMode", values.gdtfMode);
  AppendText(document, support, "FixtureID", values.fixtureId);
  AppendInteger(document, support, "FixtureIDNumeric", values.fixtureIdNumeric);
  parent->InsertEndChild(support);
}

// Appends one resolved standard MVR SceneObject node in established child
// order.
void AppendSceneObject(tinyxml2::XMLDocument &document,
                       tinyxml2::XMLElement *parent,
                       const SceneObjectValues &values) {
  tinyxml2::XMLElement *object =
      CreateObject(document, "SceneObject", values.object);
  AppendText(document, object, "Matrix",
             MatrixUtils::FormatMatrix(values.matrix));
  AppendGeometryReferences(document, object, values.geometries, false);
  AppendText(document, object, "FixtureID", values.fixtureId);
  AppendInteger(document, object, "FixtureIDNumeric", values.fixtureIdNumeric);
  parent->InsertEndChild(object);
}

// Creates the standard Layers container.
tinyxml2::XMLElement *CreateLayers(tinyxml2::XMLDocument &document) {
  return document.NewElement("Layers");
}

// Appends a standard Layer containing an already-populated ChildList.
void AppendLayer(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *layers,
                 const ObjectValues &values, tinyxml2::XMLElement *childList) {
  tinyxml2::XMLElement *layer = CreateObject(document, "Layer", values);
  layer->InsertEndChild(childList);
  layers->InsertEndChild(layer);
}

// Appends a standard GroupObject with matrix and optional populated ChildList.
void AppendGroupObject(tinyxml2::XMLDocument &document,
                       tinyxml2::XMLElement *parent, const ObjectValues &values,
                       const Matrix &matrix, tinyxml2::XMLElement *childList) {
  tinyxml2::XMLElement *group = CreateObject(document, "GroupObject", values);
  AppendText(document, group, "Matrix", MatrixUtils::FormatMatrix(matrix));
  if (childList->FirstChild())
    group->InsertEndChild(childList);
  else
    document.DeleteNode(childList);
  parent->InsertEndChild(group);
}

// Creates an unattached standard ChildList.
tinyxml2::XMLElement *CreateChildList(tinyxml2::XMLDocument &document) {
  return document.NewElement("ChildList");
}

} // namespace mvr_xml_serialization
