#include <cassert>
#include <filesystem>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "gdtf_metadata_summary.h"

namespace fs = std::filesystem;

// Writes a minimal GDTF archive containing description.xml.
static bool WriteGdtf(const fs::path &path, const std::string &descriptionXml) {
  wxFileOutputStream output(path.string());
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);
  auto *entry = new wxZipEntry("description.xml");
  entry->SetMethod(wxZIP_METHOD_DEFLATE);
  zip.PutNextEntry(entry);
  zip.Write(descriptionXml.data(), descriptionXml.size());
  zip.CloseEntry();
  zip.Close();
  return true;
}

// Verifies that the shared metadata service reads FixtureType and revision data.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path dir = fs::temp_directory_path() / "gdtf_metadata_summary_test";
  fs::create_directories(dir);
  const fs::path gdtfPath = dir / "metadata.gdtf";
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GDTF DataVersion=\"1.2\">"
      "<FixtureType Manufacturer=\"Perastage\" Description=\"Test fixture\" "
      "CreateDate=\"2026-01-02T03:04:05\">"
      "<Revisions>"
      "<Revision Text=\"Initial\" Date=\"2026-01-03T04:05:06\" "
      "UserID=\"42\" ModifiedBy=\"Tester\"/>"
      "<Revision Text=\"Updated\" Date=\"2026-01-04T05:06:07Z\" "
      "UserID=\"43\" ModifiedBy=\"Maintainer\"/>"
      "</Revisions>"
      "</FixtureType>"
      "</GDTF>";
  assert(WriteGdtf(gdtfPath, xml));

  GdtfMetadataSummary summary;
  assert(LoadGdtfMetadataSummary(gdtfPath.string(), summary));
  assert(summary.manufacturer == "Perastage");
  assert(summary.description == "Test fixture");
  assert(summary.creationDate == "2026-01-02 03:04:05");
  assert(summary.revision == "Updated");
  assert(summary.lastModified == "2026-01-04 05:06:07");
  assert(summary.userId == "43");
  assert(summary.modifiedBy == "Maintainer");
  assert(summary.version == "1.2");

  fs::remove_all(dir);
  return 0;
}
