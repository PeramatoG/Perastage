#include "fixture_gdtf_apply_services.h"

#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"

#include <algorithm>

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI fixture adapter.
gdtf::ProjectFixtureGdtfApplyServices MakeFixtureGdtfApplyServices() {
  gdtf::ProjectFixtureGdtfApplyServices services;
  services.modeExists = [](const std::filesystem::path &path,
                           const std::string &mode) {
    const auto modes = GetGdtfModes(path.string());
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
  };
  services.channelCount = [](const std::filesystem::path &path,
                             const std::string &mode) {
    return GetGdtfModeChannelCount(path.string(), mode);
  };
  services.writePhysicalProperties = [](const std::filesystem::path &path,
                                        float weightKg, float powerW,
                                        std::string &) {
    return SetGdtfProperties(path.string(), weightKg, powerW,
                             GdtfMutationAudit::BuildPerastageModifiedBy());
  };
  services.createDerivative = [](const std::filesystem::path &source,
                                 std::filesystem::path &out,
                                 std::string &diagnostic) {
    auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        {}, source.string());
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
