#include "gdtf_power_reader.h"
#include <tinyxml2.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>

namespace gdtf {
namespace {
using Element = tinyxml2::XMLElement;
std::string Attr(const Element *node, const char *name) {
  const char *value = node ? node->Attribute(name) : nullptr;
  return value ? value : "";
}
RawPowerValue Read(const Element *node, const char *attribute, std::string path) {
  RawPowerValue result{std::move(path), attribute, Attr(node, attribute), std::nullopt};
  result.watts = ParseNonnegativePower(result.text);
  return result;
}
}

GdtfPowerSnapshot ReadGdtfPower(const Element *fixtureType) {
  GdtfPowerSnapshot result;
  auto warn = [&](PowerDiagnosticCode code, const std::string &message) {
    result.diagnostics.push_back({code, message});
  };
  const auto *physical = fixtureType ? fixtureType->FirstChildElement("PhysicalDescriptions") : nullptr;
  const auto *properties = physical ? physical->FirstChildElement("Properties") : nullptr;
  for (auto *power = properties ? properties->FirstChildElement("PowerConsumption") : nullptr;
       power; power = power->NextSiblingElement("PowerConsumption")) {
    const char *attribute = power->Attribute("Value") ? "Value" :
                            power->Attribute("value") ? "value" : "PowerConsumption";
    result.properties.push_back(Read(power, attribute, "PhysicalDescriptions/Properties/PowerConsumption"));
  }
  const auto *geometries = fixtureType ? fixtureType->FirstChildElement("Geometries") : nullptr;
  std::map<std::string, std::vector<const Element *>> named;
  std::set<const Element *> referenced;
  std::vector<const Element *> references;
  std::function<void(const Element *, std::string)> collect = [&](const Element *node, std::string path) {
    if (!node) return;
    path += "/" + std::string(node->Name()) + "[" + Attr(node, "Name") + "]";
    if (!Attr(node, "Name").empty()) named[Attr(node, "Name")].push_back(node);
    const std::string kind = node->Name();
    if (kind == "GeometryReference") references.push_back(node);
    if (kind == "WiringObject" && Attr(node, "ComponentType") == "Consumer") {
      const char *attribute = node->Attribute("ElectricalPayLoad") ? "ElectricalPayLoad" : "ElectricalPayload";
      result.consumers.push_back(Read(node, attribute, path));
    }
    if (kind == "Beam" && node->Attribute("PowerConsumption"))
      result.beams.push_back(Read(node, "PowerConsumption", path));
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      collect(child, path);
  };
  if (geometries)
    for (auto *root = geometries->FirstChildElement(); root; root = root->NextSiblingElement())
      collect(root, "Geometries");
  bool safe = true;
  auto resolve = [&](const std::string &name) -> const Element * {
    const auto it = named.find(name);
    if (it == named.end() || it->second.size() != 1) { safe = false; return nullptr; }
    return it->second.front();
  };
  for (auto *reference : references)
    if (auto *target = resolve(Attr(reference, "Geometry"))) referenced.insert(target);

  std::vector<const Element *> roots;
  std::set<std::string> modeRoots;
  const auto *modes = fixtureType ? fixtureType->FirstChildElement("DMXModes") : nullptr;
  for (auto *mode = modes ? modes->FirstChildElement("DMXMode") : nullptr;
       mode; mode = mode->NextSiblingElement("DMXMode"))
    if (!Attr(mode, "Geometry").empty()) modeRoots.insert(Attr(mode, "Geometry"));
  if (modeRoots.size() == 1) {
    if (auto *root = resolve(*modeRoots.begin())) roots.push_back(root);
  } else if (modeRoots.size() > 1) {
    safe = false; // A type-wide total is not provable across different mode roots.
  } else if (geometries) {
    for (auto *root = geometries->FirstChildElement(); root; root = root->NextSiblingElement())
      if (!referenced.count(root)) roots.push_back(root);
  }
  float total = 0;
  bool beamPresent = false;
  bool beamValid = true;
  size_t consumerInstances = 0;
  size_t visits = 0;
  std::set<const Element *> active;
  std::function<void(const Element *)> expand = [&](const Element *node) {
    if (!node || ++visits > 100000 || active.size() > 256 || !active.insert(node).second) {
      safe = false; return;
    }
    const std::string kind = node->Name();
    if (kind == "WiringObject" && Attr(node, "ComponentType") == "Consumer") ++consumerInstances;
    if (kind == "Beam") {
      beamPresent = true;
      const auto value = Read(node, "PowerConsumption", "").watts;
      if (value) total += *value; else beamValid = false;
    }
    if (kind == "GeometryReference") expand(resolve(Attr(node, "Geometry")));
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement()) expand(child);
    active.erase(node);
  };
  for (auto *root : roots) expand(root);
  if (!references.empty() && roots.empty()) safe = false;
  if (!std::isfinite(total)) beamValid = false;
  if (safe && beamValid && beamPresent) result.beamTotalW = total;
  if (!safe) warn(PowerDiagnosticCode::UnresolvedGeometry, "Geometry graph cannot safely resolve a type-wide emitter total or input topology.");

  for (const auto &value : result.consumers)
    if (!value.watts || *value.watts == 0)
      warn(PowerDiagnosticCode::InvalidElectricalValue, "Consumer payload is malformed, negative or zero: " + value.text);
  for (const auto &value : result.properties)
    if (!value.watts || *value.watts == 0)
      warn(PowerDiagnosticCode::InvalidElectricalValue, "Properties power is malformed, negative or zero: " + value.text);
  for (const auto &value : result.beams)
    if (!value.watts) warn(PowerDiagnosticCode::InvalidBeamValue, "Beam power is malformed or negative: " + value.text);

  const bool ambiguousConsumers = result.consumers.size() > 1 || consumerInstances > 1 ||
                                 (!result.consumers.empty() && (!safe || consumerInstances != 1));
  if (ambiguousConsumers)
    warn(PowerDiagnosticCode::AmbiguousConsumers, "Multiple or unresolved Consumer inputs; no automatic sum is used.");
  if (result.properties.size() > 1)
    warn(PowerDiagnosticCode::AmbiguousProperties, "Multiple Properties power entries; no automatic sum is used.");
  if (!ambiguousConsumers && result.consumers.size() == 1 && result.consumers.front().watts) {
    result.planningW = result.consumers.front().watts;
    result.source = PlanningPowerSource::WiringConsumer;
  } else if (!ambiguousConsumers && result.properties.size() == 1 && result.properties.front().watts) {
    result.planningW = result.properties.front().watts;
    result.source = PlanningPowerSource::Properties;
  }
  if (result.consumers.size() == 1 && result.properties.size() == 1 &&
      result.consumers.front().watts && result.properties.front().watts) {
    const float wiring = *result.consumers.front().watts, legacy = *result.properties.front().watts;
    if (std::fabs(wiring - legacy) > std::max(0.001f, 0.01f * std::max(wiring, legacy)))
      warn(PowerDiagnosticCode::ConflictingElectricalPower, "WiringObject and Properties power differ materially; raw values are preserved.");
  }
  if (!result.planningW) warn(PowerDiagnosticCode::MissingPlanningPower, "Fixture planning/input power is not specified or is ambiguous.");
  if (result.planningW && result.beamTotalW && *result.beamTotalW > *result.planningW + 0.001f)
    warn(PowerDiagnosticCode::BeamExceedsInput, "Emitter total exceeds declared fixture input power.");
  return result;
}
} // namespace gdtf
