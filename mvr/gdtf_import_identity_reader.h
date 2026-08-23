#pragma once

#include <string>

namespace mvr::gdtf_import_identity_reader {

struct GdtfImportIdentity {
  std::string fixtureName;
  std::string manufacturer;
  std::string fixtureTypeId;
};

GdtfImportIdentity ReadGdtfImportIdentity(const std::string &gdtfPath);

} // namespace mvr::gdtf_import_identity_reader
