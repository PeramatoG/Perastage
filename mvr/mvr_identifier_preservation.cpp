#include "mvr_identifier_preservation.h"
#include "mvrscene.h"
#include <tinyxml2.h>
#include <tuple>
#include <charconv>
#include <string_view>
#include <unordered_set>

namespace mvr_identifiers {
namespace {
bool IsConsumer(const std::string &name) {
  return name == "Fixture" || name == "Truss" || name == "Support" || name == "SceneObject";
}

std::string Text(const tinyxml2::XMLElement *element) {
  std::string result;
  if (element)
    for (auto *node = element->FirstChild(); node; node = node->NextSibling())
      if (const auto *text = node->ToText()) result += text->Value();
  return result;
}

MvrExportDiagnostic Block(const std::string &uuid, const std::string &detail) {
  return {MvrExportDiagnosticCode::StandardDataPreservationBlocked,
          MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, "Identifiers", {}, uuid, {}, detail};
}
} // namespace

// Decimal conversion is checked before any narrowing or legacy fallback.
bool ParseInteger(const std::string &text, int &value) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return false;
  auto input = std::string_view(text).substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
  if (input.front() == '+') {
    input.remove_prefix(1);
    if (!input.empty() && input.front() == '-') return false;
  }
  if (input.empty()) return false;
  const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
  return result.ec == std::errc{} && result.ptr == input.data() + input.size();
}

std::optional<int> ReadOptionalInteger(const tinyxml2::XMLElement &node, const char *name) {
  const auto *element = node.FirstChildElement(name);
  int value = 0;
  return element && ParseInteger(Text(element), value) ? std::optional<int>(value) : std::nullopt;
}

void Read(const tinyxml2::XMLElement &node, std::string &text, int &numeric,
          int &unit, std::optional<MvrIdentifierSource> &source) {
  text = Text(node.FirstChildElement("FixtureID"));
  if (const auto *number = node.FirstChildElement("FixtureIDNumeric"))
    ParseInteger(Text(number), numeric);
  else
    ParseInteger(text, numeric); // Explicit compatibility for older incomplete sources.
  const auto *unitNode = node.FirstChildElement("UnitNumber");
  if (unitNode) ParseInteger(Text(unitNode), unit);
  source = MvrIdentifierSource{numeric, unitNode != nullptr,
      node.FirstChildElement("CustomId") != nullptr, node.FirstChildElement("CustomIdType") != nullptr};
}

std::optional<int> OptionalUnit(const std::optional<MvrIdentifierSource> &source,
                                int value) {
  if (value != 0 || (source && source->unitNumberPresent)) return value;
  return std::nullopt;
}

// Unsupported integer ranges and scalar shapes retain durable loss evidence.
void ScanSource(const tinyxml2::XMLElement &node, MvrScene &scene,
                const std::string &location) {
  if (!IsConsumer(node.Name())) return;
  const auto names = (std::string_view(node.Name()) == "Support" || std::string_view(node.Name()) == "Fixture" || std::string_view(node.Name()) == "Truss")
      ? std::vector<const char *>{"FixtureID", "FixtureIDNumeric", "UnitNumber", "CustomId", "CustomIdType"}
      : std::vector<const char *>{"FixtureID", "FixtureIDNumeric", "UnitNumber"};
  for (const auto *name : names) {
    const auto *element = node.FirstChildElement(name);
    if (!element) continue;
    int value = 0;
    const bool numeric = std::string_view(name) != "FixtureID";
    if (!element->FirstAttribute() && !element->FirstChildElement() &&
        !element->NextSiblingElement(name) && (!numeric || ParseInteger(Text(element), value))) continue;
    tinyxml2::XMLPrinter printer(nullptr, true);
    element->Accept(&printer);
    scene.preservationIssues.push_back({name, node.Attribute("uuid") ? node.Attribute("uuid") : "",
        location + "/" + name, printer.CStr(),
        "Identifier scalar shape or integer range cannot be represented without loss.",
        "Keep the original MVR until this representation is supported."});
  }
}

bool ValidateElement(const tinyxml2::XMLElement &node, bool requiredUnit) {
  for (const auto *name : {"FixtureID", "FixtureIDNumeric", "UnitNumber"}) {
    const auto *element = node.FirstChildElement(name);
    if (element && (element->FirstAttribute() || element->FirstChildElement() ||
                    element->NextSiblingElement(name))) return false;
  }
  int value = 0;
  const auto *numeric = node.FirstChildElement("FixtureIDNumeric");
  const auto *unit = node.FirstChildElement("UnitNumber");
  return node.FirstChildElement("FixtureID") && numeric && ParseInteger(Text(numeric), value) &&
         (!requiredUnit || unit) && (!unit || ParseInteger(Text(unit), value));
}

// Verify the serialized projection, rather than relying on visual equivalence.
std::vector<MvrExportDiagnostic> CheckProjection(
    const tinyxml2::XMLElement &root, const MvrScene &scene,
    const std::unordered_map<std::string, std::pair<std::string, int>> &ids,
    const std::unordered_map<std::string, int> &units) {
  std::vector<MvrExportDiagnostic> result;
  std::unordered_set<std::string> seen;
  const auto visit = [&](auto &&self, const tinyxml2::XMLElement &node) -> void {
    const std::string kind = node.Name();
    if (IsConsumer(kind)) {
      const std::string uuid = node.Attribute("uuid") ? node.Attribute("uuid") : "";
      const auto found = ids.find(uuid);
      const bool owns = (kind == "Fixture" && scene.fixtures.contains(uuid)) ||
          (kind == "Truss" && scene.trusses.contains(uuid)) ||
          (kind == "Support" && scene.supports.contains(uuid)) ||
          (kind == "SceneObject" && scene.sceneObjects.contains(uuid) &&
           scene.sceneObjects.at(uuid).kind == SceneObjectKind::SceneObject);
      int numeric = 0, unitValue = 0;
      const auto *unit = node.FirstChildElement("UnitNumber");
      std::optional<int> expectedUnit;
      if (kind == "Fixture" && units.contains(uuid)) expectedUnit = units.at(uuid);
      const auto unitOf = [&](const auto &objects) {
        if (const auto it = objects.find(uuid); it != objects.end())
          expectedUnit = OptionalUnit(it->second.identifierSource, it->second.unitNumber);
      };
      if (kind == "Truss") unitOf(scene.trusses);
      if (kind == "Support") unitOf(scene.supports);
      if (kind == "SceneObject") unitOf(scene.sceneObjects);
      if (kind == "Truss" && owns) {
        const auto &truss = scene.trusses.at(uuid);
        for (const auto &[name, value, present] : {
            std::tuple{"CustomId", truss.customId, truss.identifierSource && truss.identifierSource->customIdPresent},
            std::tuple{"CustomIdType", truss.customIdType, truss.identifierSource && truss.identifierSource->customIdTypePresent}}) {
          const auto actual = ReadOptionalInteger(node, name);
          if (actual.has_value() != (present || value != 0) || (actual && *actual != value))
            result.push_back(Block(uuid, "Serialized Truss custom identifier changed."));
        }
      }
      if (kind == "Support" && owns) {
        const auto &support = scene.supports.at(uuid);
        for (const auto &[name, expected] : {std::pair{"CustomIdType", support.customIdType},
                                           std::pair{"CustomId", support.customId}}) {
          const auto *element = node.FirstChildElement(name);
          if ((element != nullptr) != expected.has_value() ||
              (element && (!ParseInteger(Text(element), unitValue) || unitValue != *expected)))
            result.push_back(Block(uuid, "Serialized Support custom identifier changed."));
        }
      }
      if (!owns || !seen.insert(uuid).second || found == ids.end() ||
          !ValidateElement(node, kind == "Fixture") ||
          Text(node.FirstChildElement("FixtureID")) != found->second.first ||
          !ParseInteger(Text(node.FirstChildElement("FixtureIDNumeric")), numeric) || numeric != found->second.second ||
          (unit != nullptr) != expectedUnit.has_value() ||
          (unit && (!ParseInteger(Text(unit), unitValue) || unitValue != *expectedUnit)))
        result.push_back(Block(uuid, "Serialized identifier values or UnitNumber presence changed."));
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement()) self(self, *child);
  };
  if (const auto *sceneNode = root.FirstChildElement("Scene")) visit(visit, *sceneNode);
  if (seen.size() != ids.size()) result.push_back(Block({}, "Serialized identifier owner set changed."));
  return result;
}
} // namespace mvr_identifiers
