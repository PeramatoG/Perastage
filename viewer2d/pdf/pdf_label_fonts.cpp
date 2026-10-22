#include "pdf_label_fonts.h"
#include "projectutils.h"
#include "typography/font_resources.h"
#include <stdexcept>
namespace layout_pdf_internal {
LegacyFontPair::LegacyFontPair() {
  regular.key = "F1"; regular.family = "sans"; regular.baseName = "PerastageSans";
  bold.key = "F2"; bold.family = "sans-bold"; bold.baseName = "PerastageSansBold";
}
bool HasComposedLabels(const CommandBuffer &buffer) {
  for (const auto &cmd : buffer.commands)
    if (const auto *t = std::get_if<TextCommand>(&cmd))
      if (t->style.fontFamily.rfind("perastage-sans", 0) == 0) return true;
  return false;
}
bool LabelFonts::Prepare(std::vector<PdfObject> &objects, PdfFontCatalog &catalog, bool required) {
  if (!required) return true;
  regular.key = "F3"; regular.family = "perastage-sans"; regular.baseName = "NotoSans";
  bold.key = "F4"; bold.family = "perastage-sans-bold"; bold.baseName = "NotoSans-Bold";
  const auto root = ProjectUtils::GetResourceRoot();
  // No platform fallback for composed labels: these are the preview font files.
  if (!LoadTtfFontMetrics(typography::BundledFontPath(root, false), regular.metrics) ||
      !LoadTtfFontMetrics(typography::BundledFontPath(root, true), bold.metrics)) return false;
  AppendEmbeddedFontObjects(objects, regular);
  AppendEmbeddedFontObjects(objects, bold);
  catalog.labelRegular = &regular; catalog.labelBold = &bold;
  return Available();
}
void LabelFonts::Resources(std::ostringstream &out) const {
  if (regular.objectId) out << " /F3 " << regular.objectId << " 0 R /F4 " << bold.objectId << " 0 R";
}
}
