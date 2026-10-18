/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#include "gdtf_resolution_resources.h"
#include <set>

namespace gdtf_resolution {
namespace {
std::string NormalizeMode(const std::string &name) {
  std::string result;
  for (unsigned char ch : name) {
    if (ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch >= 128)
      result += static_cast<char>(ch);
  }
  return result;
}
}
AutomaticMode ResolveAutomaticMode(
    const Profile &profile,
    const mvr::gdtf_catalog_matcher::GdtfDownloadRequest &request,
    int patchSpacing) {
  if (!profile.usable)
    return {};
  std::set<std::string> imported(request.importedModes.begin(), request.importedModes.end());
  if (imported.size() > 1)
    return {};
  const auto name = imported.empty() ? request.requestedMode : *imported.begin();
  // An exact authored name takes priority, including names that normalize alike.
  for (const auto &mode : profile.modes)
    if (!name.empty() && mode.name == name)
      return {mode.name, ModeEvidence::ImportedName};
  const auto normalized = NormalizeMode(name);
  AutomaticMode named;
  for (const auto &mode : profile.modes) {
    if (normalized.empty() || NormalizeMode(mode.name) != normalized)
      continue;
    if (!named.mode.empty())
      return {}; // A normalized-name collision must not pick the first mode.
    named = {mode.name, ModeEvidence::ImportedName};
  }
  if (!named.mode.empty())
    return named;
  const int footprint = request.requestedFootprint > 0 ? request.requestedFootprint : patchSpacing;
  if (footprint <= 0)
    return {};
  AutomaticMode result;
  for (const auto &mode : profile.modes) {
    if (mode.footprint != footprint)
      continue;
    if (!result.mode.empty())
      return {};
    result = {mode.name, request.requestedFootprint > 0
        ? ModeEvidence::OriginalFootprint : ModeEvidence::PatchSpacing};
  }
  return result;
}
} // namespace gdtf_resolution
