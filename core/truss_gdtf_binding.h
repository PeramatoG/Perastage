/* This file is part of Perastage. */
#pragma once
#include "../models/truss.h"
#include <algorithm>
namespace truss_gdtf {
// AuxGdtf is a separate logical resource. Only an explicit reference to the same
// definition establishes legacy controlled-generation ownership.
inline bool HasOwnedAuxiliaryBinding(const Truss &truss) {
  if (truss.gdtfSpec.empty() || truss.perastageAuxGdtfArchivePath.empty()) return false;
  auto reference = [](std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.starts_with("./")) value.erase(0, 2);
    return value;
  };
  const auto standard = reference(truss.gdtfSpec);
  const auto auxiliary = reference(truss.perastageAuxGdtfArchivePath);
  if (auxiliary.find('/') != std::string::npos) return standard == auxiliary;
  return standard.substr(standard.find_last_of('/') + 1) == auxiliary;
}
inline bool PreserveStandardBinding(const Truss &truss) {
  return truss.dmxAddresses.has_value() ||
      (!truss.gdtfSpec.empty() && !HasOwnedAuxiliaryBinding(truss));
}
}
