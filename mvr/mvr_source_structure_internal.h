#pragma once
#include "mvr_source_structure_service.h"
#include "matrixutils.h"
#include <tinyxml2.h>
#include <string_view>
namespace mvr::source_structure {
using Element = tinyxml2::XMLElement;
bool ValidateType(const std::string &xml, const std::string &name,
                  const std::string &type);
std::string Xml(const Element &element);
std::string Id(const Element &element);
bool Equal(const Matrix &a, const Matrix &b);
bool Inverse(const Matrix &matrix, Matrix &inverse);
struct CurrentNode {
  bool exists = false;
  Matrix world;
  std::string parent, layer;
  std::string model;
  std::vector<GeometryInstance> geometries;
};
CurrentNode Current(const MvrScene &scene, const std::string &uuid);
Element *Find(Element *root, const std::string &uuid);
MvrExportDiagnostic
Error(const std::string &uuid, const std::string &field,
      const std::string &detail,
      MvrExportDiagnosticCode code =
          MvrExportDiagnosticCode::StandardDataPreservationBlocked);
} // namespace mvr::source_structure
