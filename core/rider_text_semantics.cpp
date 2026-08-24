#include "rider_text_semantics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <unordered_map>

namespace rider_text {
namespace {

// Trims ASCII whitespace from both ends of a string.
std::string Trim(std::string_view value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const size_t last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

// Produces an uppercase, accent-folded key with normalized whitespace.
std::string NormalizeKey(std::string_view value) {
  std::string key = Trim(value);
  while (!key.empty() && (key.back() == ':' || key.back() == ';'))
    key.pop_back();
  key = Trim(key);

  const std::array<std::pair<std::string_view, std::string_view>, 10> folds{{
      {"Á", "A"},
      {"É", "E"},
      {"Í", "I"},
      {"Ó", "O"},
      {"Ú", "U"},
      {"á", "A"},
      {"é", "E"},
      {"í", "I"},
      {"ó", "O"},
      {"ú", "U"},
  }};
  for (const auto &[accent, replacement] : folds) {
    size_t position = 0;
    while ((position = key.find(accent, position)) != std::string::npos) {
      key.replace(position, accent.size(), replacement);
      position += replacement.size();
    }
  }
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  static const std::regex whitespaceRe("\\s+");
  key = std::regex_replace(key, whitespaceRe, " ");
  return Trim(key);
}

// Removes trailing coordinate and margin commands from a heading key.
std::string RemoveCommandSuffixes(std::string value) {
  static const std::regex commandRe(
      "\\s*(?:\\([^)]*\\)|\\[[^]]*\\])\\s*");
  value = std::regex_replace(value, commandRe, " ");
  return NormalizeKey(value);
}

// Looks up an exact semantic hang alias.
std::optional<std::string> LookupHangAlias(const std::string &key) {
  static const std::unordered_map<std::string, std::string> aliases = {
      {"FRONTAL", "LX1"},
      {"FRENTE", "LX1"},
      {"PUENTE FRONTAL", "LX1"},
      {"FRONT", "LX1"},
      {"FRONT LIGHT", "LX1"},
      {"FRONT TRUSS", "LX1"},
      {"DOWNSTAGE", "LX1"},
      {"CENITAL", "LX2"},
      {"CENTRAL", "LX2"},
      {"MEDIO", "LX2"},
      {"PUENTE CENTRAL", "LX2"},
      {"MID", "LX2"},
      {"MIDDLE", "LX2"},
      {"CENTER", "LX2"},
      {"CENTRE", "LX2"},
      {"MIDSTAGE", "LX2"},
      {"CONTRA", "LX3"},
      {"CONTRALUZ", "LX3"},
      {"PUENTE TRASERO", "LX3"},
      {"TRASERO", "LX3"},
      {"BACK", "LX3"},
      {"REAR", "LX3"},
      {"BACKLIGHT", "LX3"},
      {"UPSTAGE", "LX3"},
      {"CALLE", "LX SIDES"},
      {"CALLES", "LX SIDES"},
      {"LATERAL", "LX SIDES"},
      {"LATERALES", "LX SIDES"},
      {"SIDE", "LX SIDES"},
      {"SIDES", "LX SIDES"},
      {"SIDE LIGHT", "LX SIDES"},
      {"SIDE LIGHTS", "LX SIDES"},
      {"LX SIDE", "LX SIDES"},
      {"LX SIDES", "LX SIDES"},
      {"SUELO", "FLOOR"},
      {"PISO", "FLOOR"},
      {"CALLES A SUELO", "FLOOR"},
      {"FLOOR", "FLOOR"},
      {"GROUND", "FLOOR"},
      {"DECK", "FLOOR"},
      {"GROUND LANE", "FLOOR"},
      {"GROUND LANES", "FLOOR"},
      {"PANTALLA", "SCREEN"},
      {"PANTALLA LED", "SCREEN"},
      {"PROYECCION", "SCREEN"},
      {"SCREEN", "SCREEN"},
      {"LED SCREEN", "SCREEN"},
      {"LEDSCREEN", "SCREEN"},
      {"LED WALL", "SCREEN"},
      {"PROJECTION", "SCREEN"},
  };
  const auto found = aliases.find(key);
  if (found == aliases.end())
    return std::nullopt;
  return found->second;
}

} // namespace

// Reports whether a line starts with a positive equipment quantity.
bool IsQuantityPrefixedLine(std::string_view line) {
  const std::string trimmed = Trim(line);
  size_t position = 0;
  if (position < trimmed.size() &&
      (trimmed[position] == '-' || trimmed[position] == '*')) {
    ++position;
    while (position < trimmed.size() &&
           std::isspace(static_cast<unsigned char>(trimmed[position])))
      ++position;
  }
  const size_t digitStart = position;
  while (position < trimmed.size() &&
         std::isdigit(static_cast<unsigned char>(trimmed[position])))
    ++position;
  return position > digitStart && position < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[position]));
}

// Classifies a complete rider line when it is a recognized section heading.
Section ClassifySectionHeader(std::string_view line) {
  if (IsQuantityPrefixedLine(line))
    return Section::None;
  const std::string key = NormalizeKey(line);
  if (key == "CONTROL" || key == "CONTROL DE ILUMINACION" ||
      key == "CONTROL DE LUCES" || key == "LIGHTING CONTROL" ||
      key == "CONTROL DMX")
    return Section::LightingControl;
  if (key == "ILUMINACION" || key == "ILUMIN" || key == "LIGHTING" ||
      key == "APARATOS" || key == "FIXTURES" || key == "ROBOTICA" ||
      key == "CONVENCIONALES")
    return Section::Lighting;
  if (key == "EFECTOS" || key == "EFECTO" || key == "EFFECTS" ||
      key == "EFFECT")
    return Section::Effects;
  if (key == "VIDEO" || key == "VIDEO Y PROYECCION")
    return Section::Video;
  if (key == "RIGGING" || key == "RIGGING Y ESTRUCTURAS" ||
      key == "ESTRUCTURAS" || key == "RIGGING AND STRUCTURES")
    return Section::Rigging;
  if (key == "SONIDO" || key == "AUDIO" || key == "CONTROL DE P.A." ||
      key == "MONITORES" || key == "MICROFONIA" || key == "REALIZACION")
    return Section::Ignored;
  return Section::None;
}

// Returns the canonical hang for a complete position heading.
std::optional<std::string> ClassifyHangHeader(std::string_view line) {
  if (IsQuantityPrefixedLine(line))
    return std::nullopt;
  const std::string key = RemoveCommandSuffixes(NormalizeKey(line));
  static const std::regex lxHeadingRe("LX[0-9]+");
  if (std::regex_match(key, lxHeadingRe))
    return key;
  if (key.rfind("CALLES DIRECTAS ", 0) == 0)
    return "LX SIDES";
  return LookupHangAlias(key);
}

// Normalizes a hang or rigging target alias to its canonical name.
std::string NormalizeHangAlias(std::string_view value) {
  std::string key = RemoveCommandSuffixes(NormalizeKey(value));
  static const std::regex lxTargetRe("LX[0-9]+");
  if (std::regex_match(key, lxTargetRe))
    return key;
  if (key.rfind("PUENTES ", 0) == 0)
    key = Trim(key.substr(8));
  else if (key.rfind("PUENTE ", 0) == 0)
    key = Trim(key.substr(7));
  if (const auto alias = LookupHangAlias(key))
    return *alias;
  if (key.rfind("CALLES DIRECTAS ", 0) == 0)
    return "LX SIDES";
  return key;
}

} // namespace rider_text
