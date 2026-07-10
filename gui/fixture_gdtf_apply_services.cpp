#include "fixture_gdtf_apply_services.h"

#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "filesystem_path_utils.h"

#include <algorithm>

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI fixture adapter.
gdtf::ProjectFixtureGdtfApplyServices MakeFixtureGdtfApplyServices() {
  gdtf::ProjectFixtureGdtfApplyServices services;
  services.modeExists = [](const std::filesystem::path &path,
                           const std::string &mode) {
    const auto modes = GetGdtfModes(PathUtils::PathToUtf8(path));
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
  };
  services.channelCount = [](const std::filesystem::path &path,
                             const std::string &mode) {
    return GetGdtfModeChannelCount(PathUtils::PathToUtf8(path), mode);
  };
  services.writePhysicalProperties = [](const std::filesystem::path &path,
                                        float weightKg, float powerW,
                                        std::string &) {
    return SetGdtfProperties(PathUtils::PathToUtf8(path), weightKg, powerW,
                             GdtfMutationAudit::BuildPerastageModifiedBy());
  };
  services.createDerivative = [](const std::filesystem::path &source,
                                 const std::string &fixtureType,
                                 const std::string &,
                                 std::filesystem::path &out,
                                 std::string &diagnostic) {
    if (fixtureType.empty()) {
      diagnostic = "Fixture type context is required to create a derivative.";
      return false;
    }
    auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        fixtureType, PathUtils::PathToUtf8(source));
    if (!derivative || derivative->path.empty()) {
      diagnostic = "Could not create a writable GDTF derivative.";
      return false;
    }
    out = derivative->path;
    return true;
  };
  return services;
}

} // namespace gui
