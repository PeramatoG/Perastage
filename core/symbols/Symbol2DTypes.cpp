#include "symbols/Symbol2DTypes.h"

namespace symbols {

const char *ToString(SymbolView view) {
  switch (view) {
  case SymbolView::Front:
    return "Front";
  case SymbolView::Top:
    return "Top";
  case SymbolView::Bottom:
    return "Bottom";
  case SymbolView::Left:
    return "Left";
  }
  return "Unknown";
}

std::array<SymbolView, 4> AllSymbolViews() {
  return {SymbolView::Front, SymbolView::Top, SymbolView::Bottom,
          SymbolView::Left};
}

} // namespace symbols
