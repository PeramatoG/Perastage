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
}
