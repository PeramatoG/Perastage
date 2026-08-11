#pragma once

#include <cstddef>

#include "symbolcache.h"

namespace layout_pdf_internal {

struct LayoutPdfSymbolUsage {
  std::size_t perastageSymbolInstances = 0;
  std::size_t fallbackSymbolInstances = 0;
};

LayoutPdfSymbolUsage
CountLayoutPdfSymbolUsage(const CommandBuffer &buffer,
                          const SymbolDefinitionSnapshot *symbols);

} // namespace layout_pdf_internal
