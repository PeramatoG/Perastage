#pragma once
#include "pdf_objects.h"
#include "canvas2d.h"
namespace layout_pdf_internal {
bool HasComposedLabels(const CommandBuffer &buffer);
struct LegacyFontPair {
  PdfFontDefinition regular, bold;
  LegacyFontPair();
};
struct LabelFonts {
  PdfFontDefinition regular, bold;
  bool Prepare(std::vector<PdfObject> &objects, PdfFontCatalog &catalog, bool required);
  bool Available() const { return regular.embedded && bold.embedded; }
  void Resources(std::ostringstream &out) const;
};
}
