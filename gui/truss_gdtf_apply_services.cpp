#include "truss_gdtf_apply_services.h"

#include "gdtfdictionary.h"
#include "truss_gdtf_builder.h"

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI truss adapter.
gdtf::ProjectTrussGdtfApplyServices MakeTrussGdtfApplyServices() {
  gdtf::ProjectTrussGdtfApplyServices services;
  services.canonicalFileName = [](const std::string &manufacturer,
                                  const std::string &model,
                                  const std::string &fallbackStem) {
    return GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
        manufacturer, model, fallbackStem);
  };
  services.generateGdtf = [](const Truss &truss,
                             const std::filesystem::path &outputPath,
                             const std::string &revisionText,
                             std::string &diagnostic) {
    return BuildTrussGdtfFromInstance(truss, outputPath, &diagnostic,
                                      revisionText);
  };
  return services;
}

} // namespace gui
