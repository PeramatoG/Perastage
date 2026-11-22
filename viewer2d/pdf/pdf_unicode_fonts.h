#pragma once

#include "pdf_objects.h"
#include "typography/composition.h"
#include "typography/font_resources.h"
#include <map>
#include <set>

namespace layout_pdf_internal {
// Wrap a multi-glyph cluster so PDF readers extract its original Unicode once.
void BeginActualText(std::ostringstream &, std::string_view text);

template <typename Text, typename Continuation, typename Emit>
void AppendTextClusters(std::ostringstream &out, size_t first, size_t end,
                        Text text, Continuation continuation, Emit emit) {
  while (first < end) {
    size_t next = first + 1;
    while (next < end && continuation(next)) ++next;
    if (next > first + 1) BeginActualText(out, text(first));
    for (size_t i = first; i < next; ++i) emit(i);
    if (next > first + 1) out << "EMC\n";
    first = next;
  }
}

// One registry per output document. Only used, authorized faces are embedded.
class PdfUnicodeFonts {
public:
  explicit PdfUnicodeFonts(std::shared_ptr<const typography::FontCatalog> catalog);
  const typography::FontCatalog &Catalog() const { return *catalog_; }
  void AppendGlyph(std::ostringstream &out, const FloatFormatter &fmt,
                   const typography::PositionedGlyph &glyph,
                   double x, double y, double size, double outlineRadius = 0);
  void AppendRun(std::ostringstream &out, const FloatFormatter &fmt,
                 const typography::Composition &run, double x, double y);
  void AddDiagnostic(const std::string &message);
  const std::set<std::string> &Diagnostics() const { return diagnostics_; }
  void WriteResources(std::vector<PdfObject> &objects, std::ostringstream &out) const;

private:
  struct Face {
    const typography::FontResource *resource = nullptr;
    std::string key;
    std::map<std::pair<uint16_t, std::string>, uint16_t> codes;
    std::vector<std::pair<uint16_t, std::string>> glyphs;
  };
  std::shared_ptr<const typography::FontCatalog> catalog_;
  std::map<std::string_view, Face> used_;
  std::set<std::string> diagnostics_;
};
}
