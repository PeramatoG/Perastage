#pragma once
#include <optional>
#include <cmath>
#include <charconv>
#include <string_view>
#include <string>
#include <vector>
namespace tinyxml2 { class XMLElement; }

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

} // namespace gdtf
