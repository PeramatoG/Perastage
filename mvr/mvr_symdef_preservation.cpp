/* This file is part of Perastage. */
#include "mvr_symdef_preservation.h"
#include "mvr_model_dependencies.h"
#include "filesystem_path_utils.h"
#include "matrixutils.h"
#include "uuidutils.h"
#include "file_import_utils.h"
#include <tinyxml2.h>
#include <map>
#include <unordered_set>
#include <tuple>
#include <cmath>
#include <sstream>
#include <locale>

namespace mvr {
namespace {
MvrExportDiagnostic Block(const std::string &uuid, const std::string &reason) {
  return {MvrExportDiagnosticCode::StandardDataPreservationBlocked,
          MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, "Symdef/Symbol", {}, uuid, "Scene/AUXData/Symdef",
          "MVR symbol preservation blocked (uuid=" + uuid + "): " + reason +
              " Keep the original MVR; no lossy override is available."};
}
bool ValidIdentity(const std::string &value) {
  return !value.empty() && CanonicalizeUuid(value) == value &&
         value != "00000000-0000-0000-0000-000000000000";
}
bool Equal(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w && left.o == right.o;
}
bool Finite(const Matrix &matrix) {
  for (const auto &row : {matrix.u, matrix.v, matrix.w, matrix.o})
    for (float value : row) if (!std::isfinite(value)) return false;
  return true;
}
} // namespace

// Protect source values the current float32 matrix/formatter cannot round-trip.
bool SymdefMatrixPreservesSource(const std::string &text, const Matrix &projection) {
  auto values = [](std::string input) -> std::optional<std::array<double, 12>> {
    for (char &ch : input) if (ch == '{' || ch == '}' || ch == ',') ch = ' ';
    std::istringstream stream(input);
    stream.imbue(std::locale::classic());
    std::array<double, 12> result;
    for (double &value : result) if (!(stream >> value) || !std::isfinite(value)) return std::nullopt;
    stream >> std::ws;
    if (!stream.eof()) return std::nullopt;
    return result;
  };
  const auto original = values(text);
  const auto serialized = values(MatrixUtils::FormatMatrix(projection));
  return original && serialized && original == serialized;
}

std::filesystem::path ResolveSymdefResource(const MvrScene &scene, const std::string &file) {
  auto path = PathUtils::PathFromUtf8(file);
  return path.is_absolute() ? path : PathUtils::PathFromUtf8(scene.basePath) / path;
}
std::string SymdefResourceProblem(const MvrScene &scene, const std::string &file) {
  if (file.empty()) return "Geometry3D resource reference is empty.";
  const auto path = ResolveSymdefResource(scene, file);
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) return "Geometry3D resource is unavailable: " + file;
  const auto dependencies = InspectModelDependencies(path);
  if (!dependencies.error.empty()) return dependencies.error + " Resource: " + file;
  if (!dependencies.references.empty())
    return "Externally dependent Symdef geometry needs a complete dependency merge/ownership contract: " + file;
  return {};
}

std::vector<MvrExportDiagnostic> CheckMvrSymdefs(const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> result;
  std::unordered_set<std::string> occupied;
  const auto add = [&](const auto &values) {
    for (const auto &[key, value] : values) occupied.insert(CanonicalizeUuid(key));
  };
  add(scene.classes); add(scene.positions); add(scene.layers); add(scene.fixtures);
  add(scene.trusses); add(scene.supports); add(scene.sceneObjects); add(scene.groupObjects);
  const auto projectionSymbols = [&](const auto &objects) {
    for (const auto &[uuid, object] : objects)
      for (const auto &geometry : object.geometries)
        if (!geometry.sourceSymbolUuid.empty()) occupied.insert(CanonicalizeUuid(geometry.sourceSymbolUuid));
  };
  projectionSymbols(scene.sceneObjects); projectionSymbols(scene.supports);
  const auto protectProjections = [&](const auto &objects) {
    for (const auto &[uuid, object] : objects)
      for (const auto &geometry : object.geometries)
        if (!geometry.sourceSymbolUuid.empty() || !geometry.sourceSymdefUuid.empty()) {
          result.push_back(Block(uuid, "Expanded geometric objects/supports still lack canonical Symbol instance ownership."));
          break;
        }
  };
  protectProjections(scene.sceneObjects); protectProjections(scene.supports);
  for (const auto &[uuid, definition] : scene.symdefs) {
    if (!ValidIdentity(uuid) || !occupied.insert(uuid).second)
      result.push_back(Block(uuid, "Symdef UUID must be canonical, nonnil and exclusively owned."));
    for (const auto &geometry : definition.geometries) {
      Matrix roundTrip;
      if (!MatrixUtils::ParseMatrix(MatrixUtils::FormatMatrix(geometry.transform), roundTrip) || !Equal(geometry.transform, roundTrip))
        result.push_back(Block(uuid, "Definition geometry exceeds the current matrix serialization precision."));
      if (!Finite(geometry.transform)) result.push_back(Block(uuid, "Definition geometry has a nonfinite matrix."));
      if (const auto problem = SymdefResourceProblem(scene, geometry.file); !problem.empty())
        result.push_back(Block(uuid, problem));
    }
  }
  for (const auto &[uuid, truss] : scene.trusses) {
    if (truss.sourceRepresentation != Truss::GeometryRepresentation::SymbolSymdef) continue;
    if ((!truss.sourceSymbolUuid.empty() && (!ValidIdentity(truss.sourceSymbolUuid) ||
          !occupied.insert(truss.sourceSymbolUuid).second)) || !scene.symdefs.contains(truss.sourceSymdefUuid))
      result.push_back(Block(uuid, "Truss Symbol identity must be unique and reference a retained Symdef."));
    if (!Equal(truss.sourceSymbolMatrix, MatrixUtils::Identity()))
      result.push_back(Block(uuid, "Nonidentity Symbol placement needs a separate canonical node/display transform contract."));
    if (auto definition = scene.symdefs.find(truss.sourceSymdefUuid); definition != scene.symdefs.end())
      if (!definition->second.geometries.empty() &&
          !Equal(definition->second.geometries.front().transform, MatrixUtils::Identity()))
        result.push_back(Block(uuid, "First definition geometry placement is baked into the Truss display transform."));
  }
  return result;
}

std::vector<MvrExportDiagnostic> CheckSerializedSymdefs(
    tinyxml2::XMLElement &sceneElement, const MvrScene &scene, const MvrExportOptions &options,
    const std::function<std::filesystem::path(const std::string &)> &resourceLookup) {
  std::vector<MvrExportDiagnostic> result;
  std::unordered_set<std::string> seen;
  auto *aux = sceneElement.FirstChildElement("AUXData");
  for (auto *definition = aux ? aux->FirstChildElement("Symdef") : nullptr;
       definition; definition = definition->NextSiblingElement("Symdef")) {
    const std::string uuid = definition->Attribute("uuid") ? definition->Attribute("uuid") : "";
    if (!seen.insert(uuid).second || !scene.symdefs.contains(uuid)) {
      result.push_back(Block(uuid, "Serialized definition identity differs from the canonical scene."));
      continue;
    }
    const auto &expected = scene.symdefs.at(uuid);
    std::optional<std::string> name;
    if (const auto *text = definition->Attribute("name")) name = text;
    auto *children = definition->FirstChildElement("ChildList");
    bool valid = name == expected.name && children;
    size_t index = 0;
    for (auto *geo = children ? children->FirstChildElement() : nullptr; geo; geo = geo->NextSiblingElement(), ++index) {
      Matrix matrix = MatrixUtils::Identity();
      auto *element = geo->FirstChildElement("Matrix");
      valid &= std::string(geo->Name()) == "Geometry3D" && geo->Attribute("fileName") &&
               index < expected.geometries.size();
      if (element) valid &= element->GetText() && MatrixUtils::ParseMatrix(element->GetText(), matrix);
      if (index < expected.geometries.size()) {
        valid &= Equal(matrix, expected.geometries[index].transform);
        const auto *file = geo->Attribute("fileName");
        const auto original = ResolveSymdefResource(scene, expected.geometries[index].file);
        const auto planned = file ? resourceLookup(file) : std::filesystem::path{};
        if (planned.empty()) valid = false;
        else if (PathUtils::BuildFilesystemIdentityKey(planned) != PathUtils::BuildFilesystemIdentityKey(original)) {
          const auto first = FileImportUtils::ComputeFileSha256(original);
          const auto second = FileImportUtils::ComputeFileSha256(planned);
          valid &= first && second && first == second;
        }
      }
    }
    if (!valid || index != expected.geometries.size())
      result.push_back(Block(uuid, "Serialized definition name, structure or transforms changed."));
  }
  if (seen.size() != scene.symdefs.size()) result.push_back(Block({}, "Serialization omitted a Symdef, including an unused definition."));
  const bool canonicalSymbols = options.trussGeometryExportMode == MvrTrussGeometryExportMode::Standard &&
      options.trussGeometryAuthority.value_or(MvrTrussGeometryAuthority::MvrGeometry) == MvrTrussGeometryAuthority::MvrGeometry;
  std::unordered_map<std::string, std::pair<std::string, std::string>> symbols;
  bool unique = true;
  std::unordered_map<std::string, int> identities;
  std::unordered_map<std::string, Matrix> symbolMatrices;
  const auto visit = [&](auto &&self, tinyxml2::XMLElement &node) -> void {
    if (std::string(node.Name()) == "UserData") return;
    if (auto *uuid = node.Attribute("uuid")) ++identities[uuid];
    if (std::string(node.Name()) == "Symbol") {
      const auto *uuid = node.Attribute("uuid");
      const auto *target = node.Attribute("symdef");
      auto *parent = node.Parent() ? node.Parent()->ToElement() : nullptr;
      auto *owner = parent && parent->Parent() ? parent->Parent()->ToElement() : nullptr;
      const auto *ownerUuid = owner ? owner->Attribute("uuid") : nullptr;
      Matrix matrix = MatrixUtils::Identity();
      if (auto *value = node.FirstChildElement("Matrix"))
        unique &= value->GetText() && MatrixUtils::ParseMatrix(value->GetText(), matrix);
      if (ownerUuid) symbolMatrices[ownerUuid] = matrix;
      if (!uuid || !target || !ownerUuid || !seen.contains(target) ||
          !symbols.emplace(ownerUuid, std::make_pair(uuid, target)).second) unique = false;
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement()) self(self, *child);
  };
  visit(visit, sceneElement);
  if (canonicalSymbols)
    for (const auto &[uuid, truss] : scene.trusses) {
      if (truss.sourceRepresentation != Truss::GeometryRepresentation::SymbolSymdef) continue;
      if (!symbols.contains(uuid) || symbols.at(uuid).second != truss.sourceSymdefUuid ||
          !ValidIdentity(symbols.at(uuid).first) || identities[symbols.at(uuid).first] != 1 ||
          (!truss.sourceSymbolUuid.empty() && symbols.at(uuid).first != truss.sourceSymbolUuid) ||
          !Equal(symbolMatrices.at(uuid), truss.sourceSymbolMatrix))
        result.push_back(Block(uuid, "Serialization changed a Truss Symbol identity, owner or definition reference."));
    }
  for (const auto &[uuid, definition] : scene.symdefs) unique &= identities[uuid] == 1;
  if (!unique) result.push_back(Block({}, "Serialized Symbol ownership or references are ambiguous."));
  return result;
}
} // namespace mvr
