#include "wx_path_utils.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "gdtf_fixture_category.h"
#include "gdtf_test_fixture_builder.h"

namespace fs = std::filesystem;

// Builds an intentionally incomplete GDTF description for tolerant inference.
static std::string WrapDescription(const std::string &name,
                                   const std::string &geometries,
                                   const std::string &attributes,
                                   const std::string &emitters = "",
                                   const std::string &extraFixtureContent = "") {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<GDTF DataVersion=\"1.2\">"
         "<FixtureType Name=\"" +
         name +
         "\" ShortName=\"" + name +
         "\">"
         "<AttributeDefinitions><Attributes>" +
         attributes +
         "</Attributes></AttributeDefinitions>"
         "<Geometries>" + geometries + "</Geometries>" +
         extraFixtureContent +
         "<PhysicalDescriptions><Emitters>" + emitters +
         "</Emitters></PhysicalDescriptions>"
         "</FixtureType></GDTF>";
}

// Writes a well-formed ZIP containing the supplied recovery-oriented XML.
static bool WriteGdtf(const fs::path &path, const std::string &descriptionXml) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
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

// Verifies strict, tolerant-recovery, name-only, and missing-input inference.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path dir = fs::temp_directory_path() / "gdtf_fixture_category_test";
  fs::create_directories(dir);

  struct Case {
    std::string file;
    std::string xml;
    std::string expected;
    std::string policy;
  };

  const std::vector<Case> cases = {
      {"moving_spot.gdtf",
       WrapDescription("Moving Spot",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"12\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Gobo1\"/><Attribute Name=\"Focus\"/>"),
       "Spot", "tolerant-recovery"},
      {"moving_wash.gdtf",
       WrapDescription("Moving Wash",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Wash\" BeamAngle=\"30\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Frost\"/>"),
       "Wash", "tolerant-recovery"},
      {"moving_beam.gdtf",
       WrapDescription("Moving Beam",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"3\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Prism\"/>"),
       "Beam", "tolerant-recovery"},
      {"moving_hybrid.gdtf",
       WrapDescription("Moving Hybrid",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"5\"/><Beam Name=\"Wide\" BeamType=\"Wash\" BeamAngle=\"28\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Gobo1\"/><Attribute Name=\"Focus\"/><Attribute Name=\"Frost\"/>"),
       "Hybrid", "tolerant-recovery"},
      {"name_hint_hybrid.gdtf",
       WrapDescription("Beam Wash 380",
                       "<Geometry Name=\"Root\"/>",
                       ""),
       "Hybrid", "tolerant-recovery"},
      {"name_hint_hybrid_keyword.gdtf",
       WrapDescription("Hybrid 330",
                       "<Geometry Name=\"Root\"/>",
                       ""),
       "Hybrid", "tolerant-recovery"},
      {"name_hint_smoke.gdtf",
       WrapDescription("Tour Hazer",
                       "<Geometry Name=\"Root\"/>",
                       ""),
       "Smoke", "tolerant-recovery"},
      {"empty_attributes_unknown.gdtf",
       WrapDescription("Mystery Fixture",
                       "<Geometry Name=\"Base\"/><Geometry Name=\"Yoke\"/><Geometry Name=\"Head\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"12\"/>",
                       ""),
       "Unknown", "tolerant-recovery"},
      {"moving_wash_from_dmx_channels.gdtf",
       WrapDescription("Mover 440",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Wash\" BeamAngle=\"30\"/>",
                       "",
                       "",
                       "<DMXModes><DMXMode Name=\"8ch\"><DMXChannels>"
                       "<DMXChannel Offset=\"1,1\"><LogicalChannel Attribute=\"Pan\"/></DMXChannel>"
                       "<DMXChannel Offset=\"2,1\"><LogicalChannel Attribute=\"Tilt\"/></DMXChannel>"
                       "<DMXChannel Offset=\"3,1\"><LogicalChannel Attribute=\"Zoom\"/></DMXChannel>"
                       "</DMXChannels></DMXMode></DMXModes>"),
       "Wash", "tolerant-recovery"},
      {"moving_wash_from_channel_function_attributes.gdtf",
       WrapDescription("Mover 441",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Wash\" BeamAngle=\"28\"/>",
                       "",
                       "",
                       "<DMXModes><DMXMode Name=\"8ch\"><DMXChannels>"
                       "<DMXChannel Offset=\"1,1\"><LogicalChannel><ChannelFunction Attribute=\"Pan\"/></LogicalChannel></DMXChannel>"
                       "<DMXChannel Offset=\"2,1\"><LogicalChannel><ChannelFunction Attribute=\"Tilt\"/></LogicalChannel></DMXChannel>"
                       "<DMXChannel Offset=\"3,1\"><LogicalChannel><ChannelFunction Attribute=\"Zoom\"/></LogicalChannel></DMXChannel>"
                       "</DMXChannels></DMXMode></DMXModes>"),
       "Wash", "tolerant-recovery"},
      {"static_conventional.gdtf",
       WrapDescription("Fresnel 2k",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Fresnel\" BeamAngle=\"35\"/>",
                       "<Attribute Name=\"Dimmer\"/>"),
       "Conventional", "tolerant-recovery"},
      {"pixel_led.gdtf",
       WrapDescription("LED Pixel Bar",
                       "<Geometry Name=\"Root\"/>",
                       "<Attribute Name=\"PixelMode\"/><Attribute Name=\"Color\"/>",
                       "<Emitter LampType=\"LED\"/>"),
       "LED", "tolerant-recovery"},
      {"blinder_name_fallback.gdtf",
       WrapDescription("Molefay 4",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"None\" BeamAngle=\"25\"/>",
                       "<Attribute Name=\"Dimmer\"/>"),
       "Blinder", "tolerant-recovery"},
      {"strobe.gdtf",
       WrapDescription("Atomic Strobe",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"None\" BeamAngle=\"20\"/>",
                       "<Attribute Name=\"StrobeMode\"/><Attribute Name=\"Shutter1\"/>"),
       "Strobe", "tolerant-recovery"},
      {"haze.gdtf",
       WrapDescription("Hazer 2",
                       "<Geometry Name=\"Root\"/>",
                       "<Attribute Name=\"Haze1\"/><Attribute Name=\"Blower1\"/>"),
       "Smoke", "tolerant-recovery"},
      {"laser.gdtf",
       WrapDescription("Club Laser",
                       "<Laser Name=\"Laser\"/>",
                       "<Attribute Name=\"Intensity\"/>"),
       "Laser", "tolerant-recovery"},
      {"video.gdtf",
       WrapDescription("Media Server",
                       "<Display Name=\"Display\"/>",
                       "<Attribute Name=\"VideoEffect\"/>"),
       "Video", "tolerant-recovery"},
      {"hoist.gdtf",
       WrapDescription("Chain Hoist",
                       "<Support Name=\"Support\" Type=\"Rope\"/>",
                       "<Attribute Name=\"Speed\"/>"),
       "Hoist", "tolerant-recovery"},
      {"unknown.gdtf",
       WrapDescription("Mystery Box", "<Geometry Name=\"Root\"/>", "<Attribute Name=\"Control\"/>"),
       "Unknown", "tolerant-recovery"},
  };

  for (const auto &testCase : cases) {
    const fs::path gdtfPath = dir / testCase.file;
    assert(WriteGdtf(gdtfPath, testCase.xml));
    const auto inferred = GdtfFixtureCategory::InferFromGdtf(gdtfPath.string());
    if (inferred.category != testCase.expected) {
      std::cerr << "Category inference failed: fixture=" << testCase.file
                << ", expected=" << testCase.expected
                << ", actual=" << inferred.category
                << ", reason=" << inferred.reason
                << ", policy=" << testCase.policy << '\n';
      return 1;
    }
  }

  const fs::path canonicalPath = dir / "canonical_minimal.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(canonicalPath);
  const auto canonical = GdtfFixtureCategory::InferFromGdtf(canonicalPath.string());
  if (canonical.category != "Unknown") {
    std::cerr << "Category inference failed: fixture=canonical_minimal.gdtf"
              << ", expected=Unknown, actual=" << canonical.category
              << ", reason=" << canonical.reason
              << ", policy=standard-strict\n";
    return 1;
  }

  const auto nameOnly = GdtfFixtureCategory::InferFromName("Tour Hazer");
  assert(nameOnly.category == "Smoke");
  assert(nameOnly.reason == "name hint smoke");

  const auto missing =
      GdtfFixtureCategory::InferFromGdtf((dir / "missing.gdtf").string());
  assert(missing.category == "Unknown");
  assert(missing.reason == "missing description.xml");

  fs::remove_all(dir);
  return 0;
}
