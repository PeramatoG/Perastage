#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "gdtf_fixture_category.h"

namespace fs = std::filesystem;

static std::string WrapDescription(const std::string &name,
                                   const std::string &geometries,
                                   const std::string &attributes,
                                   const std::string &emitters = "") {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<GDTF DataVersion=\"1.2\">"
         "<FixtureType Name=\"" +
         name +
         "\" ShortName=\"" + name +
         "\">"
         "<AttributeDefinitions><Attributes>" +
         attributes +
         "</Attributes></AttributeDefinitions>"
         "<Geometries>" + geometries + "</Geometries>"
         "<PhysicalDescriptions><Emitters>" + emitters +
         "</Emitters></PhysicalDescriptions>"
         "</FixtureType></GDTF>";
}

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

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path dir = fs::temp_directory_path() / "gdtf_fixture_category_test";
  fs::create_directories(dir);

  struct Case {
    std::string file;
    std::string xml;
    std::string expected;
  };

  const std::vector<Case> cases = {
      {"moving_spot.gdtf",
       WrapDescription("Moving Spot",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"12\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Gobo1\"/><Attribute Name=\"Focus\"/>"),
       "Spot"},
      {"moving_wash.gdtf",
       WrapDescription("Moving Wash",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Wash\" BeamAngle=\"30\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Frost\"/>"),
       "Wash"},
      {"moving_beam.gdtf",
       WrapDescription("Moving Beam",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"3\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Prism\"/>"),
       "Beam"},
      {"moving_hybrid.gdtf",
       WrapDescription("Moving Hybrid",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Spot\" BeamAngle=\"5\"/><Beam Name=\"Wide\" BeamType=\"Wash\" BeamAngle=\"28\"/>",
                       "<Attribute Name=\"Pan\"/><Attribute Name=\"Tilt\"/><Attribute Name=\"Gobo1\"/><Attribute Name=\"Focus\"/><Attribute Name=\"Frost\"/>"),
       "Hybrid"},
      {"static_conventional.gdtf",
       WrapDescription("Fresnel 2k",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"Fresnel\" BeamAngle=\"35\"/>",
                       "<Attribute Name=\"Dimmer\"/>"),
       "Conventional"},
      {"pixel_led.gdtf",
       WrapDescription("LED Pixel Bar",
                       "<Geometry Name=\"Root\"/>",
                       "<Attribute Name=\"PixelMode\"/><Attribute Name=\"Color\"/>",
                       "<Emitter LampType=\"LED\"/>"),
       "LED"},
      {"strobe.gdtf",
       WrapDescription("Atomic Strobe",
                       "<Geometry Name=\"Root\"/><Beam Name=\"Beam\" BeamType=\"None\" BeamAngle=\"20\"/>",
                       "<Attribute Name=\"StrobeMode\"/><Attribute Name=\"Shutter1\"/>"),
       "Strobe"},
      {"haze.gdtf",
       WrapDescription("Hazer 2",
                       "<Geometry Name=\"Root\"/>",
                       "<Attribute Name=\"Haze1\"/><Attribute Name=\"Blower1\"/>"),
       "Smoke"},
      {"laser.gdtf",
       WrapDescription("Club Laser",
                       "<Laser Name=\"Laser\"/>",
                       "<Attribute Name=\"Intensity\"/>"),
       "Laser"},
      {"video.gdtf",
       WrapDescription("Media Server",
                       "<Display Name=\"Display\"/>",
                       "<Attribute Name=\"VideoEffect\"/>"),
       "Video"},
      {"hoist.gdtf",
       WrapDescription("Chain Hoist",
                       "<Support Name=\"Support\" Type=\"Rope\"/>",
                       "<Attribute Name=\"Speed\"/>"),
       "Hoist"},
      {"unknown.gdtf",
       WrapDescription("Mystery Box", "<Geometry Name=\"Root\"/>", "<Attribute Name=\"Control\"/>"),
       "Unknown"},
  };

  for (const auto &testCase : cases) {
    const fs::path gdtfPath = dir / testCase.file;
    assert(WriteGdtf(gdtfPath, testCase.xml));
    const auto inferred = GdtfFixtureCategory::InferFromGdtf(gdtfPath.string());
    assert(inferred.category == testCase.expected);
  }

  fs::remove_all(dir);
  return 0;
}
