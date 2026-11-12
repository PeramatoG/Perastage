#include "mvr_source_structure_internal.h"
#include "uuidutils.h"
#include <set>
namespace mvr {
using namespace source_structure;
void ReconcileSourceStructure(
    MvrScene &scene,
    const std::function<std::string(const std::string &)> &uuid,
    const std::function<std::string(const std::string &)> &resource) {
  const auto remap = [&](const std::string &raw) {
    if (!uuid)
      return raw;
    const auto direct = uuid(raw);
    return direct != raw ? direct : uuid(CanonicalizeUuid(raw));
  };
  if (uuid)
    for (auto &issue : scene.preservationIssues)
      if (CanRestoreSourceIssue(scene, issue))
        issue.objectUuid = remap(issue.objectUuid);
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes) {
        if (uuid) {
          node.uuid = remap(node.uuid);
          node.parentUuid = remap(node.parentUuid);
          node.layerUuid = remap(node.layerUuid);
          node.projectedParentUuid = remap(node.projectedParentUuid);
        }
        if (resource && node.geometryAuthority && !node.projectedModel.empty())
          node.projectedModel = resource(node.projectedModel);
        for (auto &geometry : node.projectedGeometries) {
          if (resource && node.geometryAuthority)
            geometry.modelFile = resource(geometry.modelFile);
          if (uuid) {
            geometry.sourceSymbolUuid = remap(geometry.sourceSymbolUuid);
            geometry.sourceSymdefUuid = remap(geometry.sourceSymdefUuid);
          }
        }
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS)
          continue;
        const auto visit = [&](auto &&self, Element &part,
                               bool geometryScope) -> void {
          geometryScope |= std::string(part.Name()) == "Geometries";
          if (uuid)
            for (const char *key :
                 {"uuid", "symdef", "multipatch", "toObject", "linkedDef"})
              if (const char *value = part.Attribute(key))
                part.SetAttribute(key, remap(value).c_str());
          const std::string name = part.Name();
          if (uuid &&
              (name == "Classing" || name == "Focus" || name == "Position") &&
              part.GetText())
            part.SetText(remap(part.GetText()).c_str());
          if (resource && (!node.modeled || geometryScope)) {
            const auto rewrite = [&](const std::string &value,
                                     const char *attribute) {
              const auto mapped = resource(value);
              if (mapped.empty())
                scene.preservationIssues.push_back(
                    {"Resource", node.uuid, name, value,
                     "Source structure resource could not be copied during "
                     "merge.",
                     "Restore the resource.",
                     MvrPreservationIssueKind::ResourceFailure});
              else if (attribute)
                part.SetAttribute(attribute, mapped.c_str());
              else
                part.SetText(mapped.c_str());
            };
            if (const char *file = part.Attribute("fileName"))
              rewrite(file, "fileName");
            if ((name == "GDTFSpec" ||
                 (name == "Source" && part.Attribute("type") &&
                  std::string(part.Attribute("type")) == "File")) &&
                part.GetText())
              rewrite(part.GetText(), nullptr);
          }
          for (auto *child = part.FirstChildElement(); child;
               child = child->NextSiblingElement())
            self(self, *child, geometryScope);
        };
        visit(visit, *xml.RootElement(), false);
        node.xml = Xml(*xml.RootElement());
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
}
std::vector<SymdefGeometry> ExpandSymdefGeometry(const MvrScene &scene,
                                                 const std::string &uuid) {
  std::vector<SymdefGeometry> result;
  std::set<std::string> visiting;
  const auto expand = [&](auto &&self, const std::string &id,
                          const Matrix &placement) -> void {
    auto found = scene.symdefs.find(id);
    if (found == scene.symdefs.end() || !visiting.insert(id).second)
      return;
    for (auto geometry : found->second.geometries) {
      geometry.transform = MatrixUtils::Multiply(placement, geometry.transform);
      result.push_back(std::move(geometry));
    }
    for (const auto &symbol : found->second.symbols)
      self(self, symbol.symdef,
           MatrixUtils::Multiply(placement, symbol.transform));
    visiting.erase(id);
  };
  expand(expand, CanonicalizeUuid(uuid), Matrix{});
  return result;
}
} // namespace mvr
