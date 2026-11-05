#pragma once
#include "layouts/PageOutputMapping.h"
#include "pdf_objects.h"
namespace layout_pdf_internal {
// Convert top-left/y-down document fit to the PDF bottom-left coordinate space.
// Existing renderers express y as outputPageHeight - documentY before this CTM.
inline void BeginDocumentPageMapping(std::ostringstream &out, double outputPageHeight,
    double ratioX, double ratioY, const FloatFormatter &formatter) {
  const auto mapping = layouts::composition::UniformPageFit(ratioX,ratioY);
  out << "q\n" << formatter.Format(mapping.scale) << " 0 0 "
      << formatter.Format(mapping.scale) << " 0 "
      << formatter.Format(outputPageHeight * (1-mapping.scale)) << " cm\n";
}
// Every element owns one complete output group, including its clip and border.
class ScopedDocumentPageMapping {
public:
  ScopedDocumentPageMapping(std::ostringstream &out, double pageHeight,
      double ratioX, double ratioY, const FloatFormatter &formatter) : out_(out) {
    BeginDocumentPageMapping(out_,pageHeight,ratioX,ratioY,formatter);
  }
  ~ScopedDocumentPageMapping() { out_ << "Q\n"; }
  ScopedDocumentPageMapping(const ScopedDocumentPageMapping &) = delete;
  ScopedDocumentPageMapping &operator=(const ScopedDocumentPageMapping &) = delete;
private:
  std::ostringstream &out_;
};

// Clip the mapped source page, including content outside authored frames. This
// clip uses final PDF coordinates and introduces no second output transform.
class ScopedDocumentPageClip {
public:
  ScopedDocumentPageClip(std::ostringstream &out, double width, double height,
      double ratioX, double ratioY, const FloatFormatter &formatter) : out_(out) {
    const double scale = layouts::composition::UniformPageFit(ratioX,ratioY).scale;
    const double mappedW = (width/ratioX)*scale, mappedH = (height/ratioY)*scale;
    out_ << "q\n0 " << formatter.Format(height-mappedH) << ' '
         << formatter.Format(mappedW) << ' ' << formatter.Format(mappedH) << " re W n\n";
  }
  ~ScopedDocumentPageClip() { out_ << "Q\n"; }
  ScopedDocumentPageClip(const ScopedDocumentPageClip &) = delete;
  ScopedDocumentPageClip &operator=(const ScopedDocumentPageClip &) = delete;
private:
  std::ostringstream &out_;
};

}
