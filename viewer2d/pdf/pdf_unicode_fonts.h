#pragma once

#include "pdf_objects.h"
#include "typography/composition.h"
#include "typography/font_resources.h"
#include <map>
#include <set>

namespace layout_pdf_internal {
// Preserve multi-glyph clusters and interword spaces as explicit source text.
void BeginActualText(std::ostringstream &, std::string_view text);

template <typename Text, typename Continuation, typename SameLeftToRightLine, typename Emit>
void AppendTextClusters(std::ostringstream &out, size_t first, size_t end,
                        Text text, Continuation continuation,
                        SameLeftToRightLine sameLine, Emit emit) {
  const auto isSpace = [&](size_t i) { return text(i) == " " || text(i) == "\t"; };
  while (first < end) {
    size_t next = first + 1;
    while (next < end && continuation(next)) ++next;
    std::string original;
    if (next > first + 1) original = text(first);
    // An isolated space in its own PDF text object can be dropped by readers.
    // Include adjacent ink in this span while keeping the original positions.
    while (!isSpace(first) && next < end && isSpace(next) && sameLine(first, next)) {
      if (original.empty()) original = text(first);
      do { original += text(next++); } while (next < end && isSpace(next) && sameLine(first, next));
      if (next == end || !sameLine(first, next) || (next + 1 < end && continuation(next + 1))) break;
      original += text(next++);
    }
    if (!original.empty()) BeginActualText(out, original);
    for (size_t i = first; i < next; ++i) emit(i);
    if (!original.empty()) out << "EMC\n";
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
    std::shared_ptr<const typography::FontResource> resource;
    std::string key;
    std::map<std::pair<uint16_t, std::string>, uint16_t> codes;
    std::vector<std::pair<uint16_t, std::string>> glyphs;
  };
  std::shared_ptr<const typography::FontCatalog> catalog_;
  std::map<std::string, Face, std::less<>> used_;
  std::set<std::string> diagnostics_;
};
}
