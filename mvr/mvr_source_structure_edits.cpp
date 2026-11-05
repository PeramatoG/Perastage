#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace mvr {
using namespace source_structure;
namespace {
bool SameGeometry(const std::vector<GeometryInstance> &a,
                  const std::vector<GeometryInstance> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].modelFile != b[i].modelFile ||
        !Equal(a[i].localTransform, b[i].localTransform) ||
        a[i].sourceSymbolUuid != b[i].sourceSymbolUuid ||
        a[i].sourceSymdefUuid != b[i].sourceSymdefUuid)
      return false;
  return true;
}
std::string NewDefinition(MvrScene &scene, const std::string &owner,
                          const std::string &symbol,
                          std::vector<SymdefGeometry> geometries) {
  std::ostringstream seed;
  seed << "edited MVR geometry:" << owner << ':' << symbol;
  for (const auto &geometry : geometries)
    seed << geometry.file << ':'
         << FormatCanonicalMatrix(geometry.transform, geometry.matrixSource)
         << ';';
  auto id = DeriveDeterministicUuid(seed.str());
  size_t suffix = 0;
  const auto reserved = SourceStructureIdentities(scene);
  const auto occupied = [&](const std::string &value) {
    if (scene.symdefs.contains(value) || scene.layers.contains(value) ||
        scene.classes.contains(value) || scene.positions.contains(value) ||
        scene.fixtures.contains(value) || scene.trusses.contains(value) ||
        scene.supports.contains(value) || scene.sceneObjects.contains(value) ||
        scene.groupObjects.contains(value))
      return true;
    if (std::find(reserved.begin(), reserved.end(), value) != reserved.end())
      return true;
    for (const auto &[key, definition] : scene.symdefs)
      for (const auto &instance : definition.symbols)
        if (instance.uuid == value)
          return true;
    return false;
  };
  while (occupied(id))
    id = DeriveDeterministicUuid(seed.str() + ':' + std::to_string(++suffix));
  SymdefDefinition definition;
  definition.name = "Edited geometry";
  definition.geometries = std::move(geometries);
  scene.symdefs.emplace(id, std::move(definition));
  return id;
}
} // namespace
void PrepareSourceStructureEdits(MvrScene &scene) {
  for (auto &provenance : scene.sourceStructures)
    if (provenance) {
      std::shared_ptr<MvrSourceStructure> changed;
      for (size_t index = 0; index < provenance->nodes.size(); ++index) {
        const auto &original = provenance->nodes[index];
        if (!original.geometryAuthority || !original.modeled)
          continue;
        auto current = Current(scene, original.uuid);
        if (!current.exists)
          continue;
        const bool truss = original.kind == "Truss";
        if (truss ? current.model == original.projectedModel
                  : SameGeometry(current.geometries,
                                 original.projectedGeometries))
          continue;
        if (!changed)
          changed = std::make_shared<MvrSourceStructure>(*provenance);
        auto &node = changed->nodes[index];
        node.editedGeometry = true;
        tinyxml2::XMLDocument xml;
        xml.Parse(node.xml.c_str());
        auto *geometries = xml.RootElement()->FirstChildElement("Geometries");
        if (!geometries)
          continue;
        if (truss) {
          if (auto *direct = geometries->FirstChildElement("Geometry3D"))
            direct->SetAttribute("fileName", current.model.c_str());
          else if (auto *symbol = geometries->FirstChildElement("Symbol")) {
            auto expanded = ExpandSymdefGeometry(
                scene,
                symbol->Attribute("symdef") ? symbol->Attribute("symdef") : "");
            if (expanded.empty())
              expanded.push_back({current.model, Matrix{}, std::nullopt});
            else
              expanded.front().file = current.model;
            auto id = NewDefinition(scene, node.uuid, Id(*symbol),
                                    std::move(expanded));
            symbol->SetAttribute("symdef", id.c_str());
            scene.trusses.at(node.uuid).sourceSymdefUuid = id;
          }
        } else {
          // A symbol instance keeps its UUID. Only its edited geometry gets an
          // isolated definition; original (including unused) definitions
          // survive.
          std::unordered_set<std::string> retained;
          std::vector<Element *> symbols;
          for (auto *symbol = geometries->FirstChildElement("Symbol"); symbol;
               symbol = symbol->NextSiblingElement("Symbol"))
            symbols.push_back(symbol);
          for (auto *symbol : symbols) {
            const auto id = Id(*symbol);
            retained.insert(id);
            std::vector<GeometryInstance> before, after;
            for (const auto &value : node.projectedGeometries)
              if (value.sourceSymbolUuid == id)
                before.push_back(value);
            for (const auto &value : current.geometries)
              if (value.sourceSymbolUuid == id)
                after.push_back(value);
            if (SameGeometry(before, after))
              continue;
            Matrix placement;
            auto *matrix = symbol->FirstChildElement("Matrix");
            if (matrix)
              if (auto source = ReadCanonicalMatrixElement(*matrix))
                placement = source->projection;
            Matrix inverse;
            const bool invertible = Inverse(placement, inverse);
            if (!invertible && matrix)
              matrix->SetText(
                  FormatCanonicalMatrix(Matrix{}, std::nullopt).c_str());
            std::vector<SymdefGeometry> local;
            for (const auto &value : after)
              local.push_back({value.modelFile,
                               invertible ? MatrixUtils::Multiply(
                                                inverse, value.localTransform)
                                          : value.localTransform,
                               std::nullopt});
            auto definition =
                NewDefinition(scene, node.uuid, id, std::move(local));
            symbol->SetAttribute("symdef", definition.c_str());
          }
          std::vector<Element *> direct;
          for (auto *geometry = geometries->FirstChildElement("Geometry3D");
               geometry; geometry = geometry->NextSiblingElement("Geometry3D"))
            direct.push_back(geometry);
          std::unordered_map<std::string, std::string> directSources;
          size_t directIndex = 0;
          for (const auto &value : node.projectedGeometries)
            if (value.sourceSymbolUuid.empty() && directIndex < direct.size())
              directSources[value.instanceKey] = Xml(*direct[directIndex++]);
          for (auto *geometry : direct)
            geometries->DeleteChild(geometry);
          tinyxml2::XMLNode *previous = nullptr;
          for (const auto &value : current.geometries)
            if (!retained.contains(value.sourceSymbolUuid)) {
              tinyxml2::XMLDocument authored;
              if (directSources.contains(value.instanceKey))
                authored.Parse(directSources.at(value.instanceKey).c_str());
              auto *geometry =
                  authored.RootElement()
                      ? authored.RootElement()->DeepClone(&xml)->ToElement()
                      : xml.NewElement("Geometry3D");
              geometry->SetAttribute("fileName", value.modelFile.c_str());
              auto *matrix = geometry->FirstChildElement("Matrix");
              const auto precision =
                  matrix ? ReadCanonicalMatrixElement(*matrix) : std::nullopt;
              if (!matrix) {
                matrix = xml.NewElement("Matrix");
                geometry->InsertFirstChild(matrix);
              }
              matrix->SetText(
                  FormatCanonicalMatrix(value.localTransform, precision)
                      .c_str());
              if (previous)
                geometries->InsertAfterChild(previous, geometry);
              else
                geometries->InsertFirstChild(geometry);
              previous = geometry;
            }
        }
        node.xml = Xml(*xml.RootElement());
        node.projectedGeometries = current.geometries;
        node.projectedModel = current.model;
      }
      if (changed)
        provenance = std::move(changed);
    }
}
} // namespace mvr
