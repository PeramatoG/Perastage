#include "gdtf_resolution_resources.h"
#include "gdtf_import_matching.h"
#include "runtime_storage.h"
#include "support/archive_entry_test_utils.h"
#include <wx/init.h>
#include <cassert>
#include <fstream>

namespace fs = std::filesystem;
using namespace gdtf_resolution;

int main() {
  wxInitializer initialize;
  assert(initialize.IsOk());
  runtime_storage::TemporaryWorkspace workspace("gdtf-resolution-test");
  const auto source = workspace.Path() / "original.gdtf";
  const std::string xml = R"(<GDTF DataVersion="1.2"><FixtureType Name="MAC Aura XB"
      Manufacturer="Martin" FixtureTypeID="00000000-0000-4000-8000-000000000001">
      <DMXModes><DMXMode Name="Standard"><DMXChannels>
      <DMXChannel Offset="1,2" Geometry="Head"/>
      </DMXChannels></DMXMode><DMXMode Name="Extended"/></DMXModes>
      </FixtureType></GDTF>)";
  std::string diagnostic;
  assert(tests::archive::WriteStoredZipWithRawNames(source, {{"description.xml",xml}}, diagnostic));
  const auto originalBytes = tests::archive::ReadBinaryFile(source.string());
  auto profile = ReadProfile(source);
  assert(profile.usable && profile.manufacturer == "Martin" && profile.model == "MAC Aura XB");
  assert(profile.modes.size() == 2 && profile.modes.front().footprint == 2);
  assert(HasMode(profile, "Standard") && !HasMode(profile, "standard"));
  fs::create_directory(workspace.Path() / "scene");
  assert(CopyIntoScene(source, {}, diagnostic).empty());
  const auto destination = CopyIntoScene(source, workspace.Path() / "scene", diagnostic);
  assert(!destination.empty() && destination != source);
  assert(tests::archive::ReadBinaryFile(destination.string()) == originalBytes);
  assert(tests::archive::ReadBinaryFile(source.string()) == originalBytes);
  auto request = mvr::gdtf_import_matching::BuildDownloadRequest(
      "Instance", "Martin Mac Aura XB", "Standard", "Custom", 2);
  assert(AutomaticProfileCompatible(profile, request, "Standard"));
  assert(!AutomaticProfileCompatible(profile, request, "Extended"));
  request.manufacturer = "Other";
  assert(!AutomaticProfileCompatible(profile, request, "Standard"));
  request.manufacturer = "Martin Professional";
  assert(AutomaticProfileCompatible(profile, request, "Standard"));
  request.authoritativeFixtureNames = {"MAC Aura"};
  assert(!AutomaticProfileCompatible(profile, request, "Standard"));

  assert(!ReadProfile(workspace.Path() / "missing.gdtf").usable);
  const auto broken = workspace.Path() / "broken.gdtf";
  std::ofstream(broken) << "not a ZIP";
  assert(!ReadProfile(broken).usable && !ReadProfile(broken).diagnostics.empty());
  const auto malformed = workspace.Path() / "malformed.gdtf";
  assert(tests::archive::WriteStoredZipWithRawNames(malformed,
      {{"description.xml", "<GDTF><FixtureType"}}, diagnostic));
  assert(!ReadProfile(malformed).usable);
  const auto noModes = workspace.Path() / "no-modes.gdtf";
  assert(tests::archive::WriteStoredZipWithRawNames(noModes,
      {{"description.xml", "<GDTF><FixtureType Name='Missing modes'/></GDTF>"}}, diagnostic));
  assert(!ReadProfile(noModes).usable);
  const auto duplicate = workspace.Path() / "duplicate-mode.gdtf";
  assert(tests::archive::WriteStoredZipWithRawNames(duplicate,
      {{"description.xml", "<GDTF><FixtureType Name='Duplicate'><DMXModes>"
       "<DMXMode Name='Same'/><DMXMode Name='Same'/></DMXModes></FixtureType></GDTF>"}}, diagnostic));
  assert(!ReadProfile(duplicate).usable);
  std::ofstream(workspace.Path() / "not-a-directory") << "file";
  assert(CopyIntoScene(source, workspace.Path() / "not-a-directory", diagnostic).empty());
  assert(!diagnostic.empty());
}
