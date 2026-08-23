#include "gdtf_import_identity_reader.h"

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "uuidutils.h"

namespace mvr::gdtf_import_identity_reader {

// Reads standard fixture identity evidence through the shared GDTF readers.
GdtfImportIdentity ReadGdtfImportIdentity(const std::string &gdtfPath) {
  GdtfImportIdentity identity;
  const auto archive = gdtf::ReadGdtfArchive(gdtfPath);
  if (!archive.Success())
    return identity;
  const auto description = gdtf::ReadGdtfDescription(archive.descriptionXml);
  if (!description.Success())
    return identity;
  identity.fixtureName = description.fixtureTypeName;
  identity.manufacturer = description.manufacturer;
  identity.fixtureTypeId = CanonicalizeUuid(description.fixtureTypeId);
  if (identity.fixtureTypeId.empty())
    identity.fixtureTypeId = description.fixtureTypeId;
  return identity;
}

} // namespace mvr::gdtf_import_identity_reader
