#pragma once
#include <optional>
#include <cmath>
#include <charconv>
#include <string_view>
#include <set>
#include <string>
#include <vector>
#include <tinyxml2.h>

namespace gdtf {
inline std::optional<float> ParseNonnegativePower(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) return std::nullopt;
  text = text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
  if (!text.empty() && text.front() == '+') text.remove_prefix(1);
  float value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value) && value >= 0)
    return value;
  return std::nullopt;
}

enum class PlanningPowerSource { Unknown, WiringConsumer, Properties };
enum class PowerDiagnosticCode {
  MissingPlanningPower, AmbiguousConsumers, AmbiguousProperties,
  ConflictingElectricalPower, BeamExceedsInput, InvalidElectricalValue,
  InvalidBeamValue, UnresolvedGeometry
};
inline const char *ToString(PowerDiagnosticCode code) {
  switch (code) {
  case PowerDiagnosticCode::MissingPlanningPower: return "missing_planning_power";
  case PowerDiagnosticCode::AmbiguousConsumers: return "ambiguous_consumers";
  case PowerDiagnosticCode::AmbiguousProperties: return "ambiguous_properties";
  case PowerDiagnosticCode::ConflictingElectricalPower: return "conflicting_electrical_power";
  case PowerDiagnosticCode::BeamExceedsInput: return "beam_exceeds_input";
  case PowerDiagnosticCode::InvalidElectricalValue: return "invalid_electrical_value";
  case PowerDiagnosticCode::InvalidBeamValue: return "invalid_beam_value";
  case PowerDiagnosticCode::UnresolvedGeometry: return "unresolved_geometry";
  }
  return "unknown";
}
inline const char *ToString(PlanningPowerSource source) {
  switch (source) {
  case PlanningPowerSource::WiringConsumer: return "wiring_consumer";
  case PlanningPowerSource::Properties: return "properties";
  case PlanningPowerSource::Unknown: return "unknown";
  }
  return "unknown";
}
struct PowerDiagnostic {
  PowerDiagnosticCode code;
  std::string message;
};
struct RawPowerValue {
  std::string path;
  std::string attribute;
  std::string text;
  std::optional<float> watts;
};
struct GdtfPowerSnapshot {
  std::optional<float> planningW;
  std::optional<float> beamTotalW;
  PlanningPowerSource source = PlanningPowerSource::Unknown;
  std::vector<RawPowerValue> consumers;
  std::vector<RawPowerValue> properties;
  std::vector<RawPowerValue> beams;
  std::vector<PowerDiagnostic> diagnostics;
};
GdtfPowerSnapshot ReadGdtfPower(const tinyxml2::XMLElement *fixtureType);

// Conservative shared write target: never manufactures connector/topology data.
// Multiple authored consumers or instances of an input subtree are unsafe.
inline tinyxml2::XMLElement *FindPowerWriteTarget(
    tinyxml2::XMLElement *fixtureType, const char *&attribute) {
  std::vector<tinyxml2::XMLElement *> consumers;
  std::set<std::string> referenceTargets;
  std::set<std::string> consumerAncestors;
  auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (!node) return;
    if (std::string(node->Name()) == "GeometryReference" && node->Attribute("Geometry"))
      referenceTargets.insert(node->Attribute("Geometry"));
    if (std::string(node->Name()) == "WiringObject" &&
        node->Attribute("ComponentType", "Consumer")) {
      consumers.push_back(node);
      for (auto *ancestor = node; ancestor; ancestor = ancestor->Parent() ? ancestor->Parent()->ToElement() : nullptr)
        if (ancestor->Attribute("Name")) consumerAncestors.insert(ancestor->Attribute("Name"));
    }
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) self(self, child);
  };
  visit(visit, fixtureType ? fixtureType->FirstChildElement("Geometries") : nullptr);
  bool instancedConsumer = false;
  for (const auto &name : referenceTargets)
    if (consumerAncestors.count(name)) instancedConsumer = true;
  if (consumers.size() == 1 && !instancedConsumer) {
    attribute = "ElectricalPayLoad";
    return consumers.front();
  }
  auto *physical = fixtureType ? fixtureType->FirstChildElement("PhysicalDescriptions") : nullptr;
  auto *properties = physical ? physical->FirstChildElement("Properties") : nullptr;
  auto *power = properties ? properties->FirstChildElement("PowerConsumption") : nullptr;
  if (power && !power->NextSiblingElement("PowerConsumption") &&
      power->Attribute("Connector") && *power->Attribute("Connector") &&
      power->Attribute("Value")) {
    if (ParseNonnegativePower(power->Attribute("Value")) &&
        std::string_view(power->Attribute("Connector")).find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%()*+-/:;<=>@_` \"'") == std::string_view::npos) {
      attribute = "Value";
      return power;
    }
  }
  attribute = nullptr;
  return nullptr;
}
} // namespace gdtf
