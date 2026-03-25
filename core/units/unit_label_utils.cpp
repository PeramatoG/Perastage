#include "unit_label_utils.h"

namespace Units {

std::string LabelWithUnit(const std::string &label, const std::string &unitSuffix) {
  return label + " (" + unitSuffix + ")";
}

} // namespace Units
