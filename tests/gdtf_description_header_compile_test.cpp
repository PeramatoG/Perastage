#include "gdtf_description_reader.h"

#ifdef TINYXML2_INCLUDED
#error "GDTF description snapshots must not expose private XML parser headers."
#endif

// Compiles the public snapshot using only standard library and Core headers.
int main() {
  const gdtf::GdtfDescriptionSnapshot description;
  return description.electricalPower.planningW.has_value() ? 1 : 0;
}
