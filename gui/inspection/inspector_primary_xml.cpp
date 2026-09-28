#include "inspection/inspector_primary_xml.h"

namespace gui::inspection {

// Returns the authoritative root XML entry already shown for one source.
std::optional<std::string>
InspectorPrimaryXmlEntry(const DisplayedPackageContext &context) {
  if (context.mvr && context.mvr->snapshot)
    return context.mvr->snapshot->sceneDescriptionEntry;
  if (context.gdtf && context.gdtf->document)
    return context.gdtf->document->Archive().descriptionEntryPath;
  return std::nullopt;
}

} // namespace gui::inspection
