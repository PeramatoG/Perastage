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
#include "mvr_xml_document_writer.h"

#include "matrixutils.h"

#include <tinyxml2.h>

namespace mvr_xml_serialization {

// Creates the fixed MVR 1.6 document envelope in its established order.
tinyxml2::XMLElement *CreateDocument(tinyxml2::XMLDocument &document,
                                     const std::string &providerVersion) {
  document.InsertEndChild(
      document.NewDeclaration("xml version=\"1.0\" encoding=\"UTF-8\""));
  tinyxml2::XMLElement *root = document.NewElement("GeneralSceneDescription");
  root->SetAttribute("verMajor", 1);
  root->SetAttribute("verMinor", 6);
  root->SetAttribute("provider", "Perastage");
  root->SetAttribute("providerVersion", providerVersion.c_str());
  document.InsertEndChild(root);
  return root;
}

// Appends and returns the standard Scene node after any root UserData.
tinyxml2::XMLElement *AppendScene(tinyxml2::XMLDocument &document,
                                  tinyxml2::XMLElement *root) {
  tinyxml2::XMLElement *scene = document.NewElement("Scene");
  root->InsertEndChild(scene);
  return scene;
}

// Writes prepared Position UUID and name values without further normalization.
tinyxml2::XMLElement *AppendPreparedPositions(
    tinyxml2::XMLDocument &document,
    const std::unordered_map<std::string, std::string> &positions) {
  tinyxml2::XMLElement *aux = document.NewElement("AUXData");
  for (const auto &[uuid, name] : positions) {
    tinyxml2::XMLElement *position = document.NewElement("Position");
    position->SetAttribute("uuid", uuid.c_str());
    if (!name.empty())
      position->SetAttribute("name", name.c_str());
    aux->InsertEndChild(position);
  }
  return aux;
}

// Appends a Symdef and its resolved Geometry3D children in MVR order.
void AppendSymdef(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *aux,
                  const SymdefValues &values) {
  tinyxml2::XMLElement *symdef = document.NewElement("Symdef");
  symdef->SetAttribute("uuid", values.uuid.c_str());
  if (!values.geometryType.empty())
    symdef->SetAttribute("geometryType", values.geometryType.c_str());
  if (!values.geometries.empty()) {
    tinyxml2::XMLElement *children = document.NewElement("ChildList");
    for (const SymdefGeometryValues &value : values.geometries) {
      tinyxml2::XMLElement *geometry = document.NewElement("Geometry3D");
      geometry->SetAttribute("fileName", value.fileName.c_str());
      if (!value.geometryType.empty())
        geometry->SetAttribute("geometryType", value.geometryType.c_str());
      tinyxml2::XMLElement *matrix = document.NewElement("Matrix");
      matrix->SetText(MatrixUtils::FormatMatrix(value.matrix).c_str());
      geometry->InsertEndChild(matrix);
      children->InsertEndChild(geometry);
    }
    if (children->FirstChild())
      symdef->InsertEndChild(children);
  }
  aux->InsertEndChild(symdef);
}

} // namespace mvr_xml_serialization
