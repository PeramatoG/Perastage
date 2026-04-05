/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "../core/app_version.h"
#include "../core/gdtf_mutation_audit.h"
#include "../viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace {

std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string content;
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    content.append(buffer, bytes);
  }
  return content;
}

std::string MakeBaseGdtf() {
  wxFileName tempName(wxFileName::CreateTempFileName("gdtf_set_color_"));
  const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
  wxRemoveFile(tempName.GetFullPath());

  wxFFileOutputStream fileOut(outPath);
  assert(fileOut.IsOk());
  wxZipOutputStream zipOut(fileOut);

  zipOut.PutNextEntry("description.xml");
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GDTF DataVersion=\"1.2\">"
      "<FixtureType Name=\"SetColorFixture\" Manufacturer=\"Acme\" Editor=\"Vendor\">"
      "<Models>"
      "<Model Name=\"Body\" File=\"\" PrimitiveType=\"Cube\" Length=\"1\" Width=\"1\" Height=\"1\"/>"
      "<Model Name=\"Yoke\" File=\"\" PrimitiveType=\"Cube\" Length=\"1\" Width=\"1\" Height=\"1\"/>"
      "</Models>"
      "<Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>"
      "</FixtureType>"
      "</GDTF>";
  zipOut.Write(xml.data(), xml.size());
  zipOut.Close();

  return outPath;
}

std::string HexToCieForAssertion(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return {};

  unsigned int rgb = 0;
  std::istringstream iss(hex.substr(1));
  iss >> std::hex >> rgb;

  const unsigned int R = (rgb >> 16) & 0xFF;
  const unsigned int G = (rgb >> 8) & 0xFF;
  const unsigned int B = rgb & 0xFF;

  auto invGamma = [](double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
  };

  const double r = invGamma(R / 255.0);
  const double g = invGamma(G / 255.0);
  const double b = invGamma(B / 255.0);

  const double X = 0.4124 * r + 0.3576 * g + 0.1805 * b;
  const double Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  const double Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;

  const double sum = X + Y + Z;
  double x = 0.0;
  double y = 0.0;
  if (sum > 0.0) {
    x = X / sum;
    y = Y / sum;
  }

  std::ostringstream cie;
  cie.setf(std::ios::fixed, std::ios::floatfield);
  cie.precision(6);
  cie << x << "," << y << "," << Y;
  return cie.str();
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::string gdtfPath = MakeBaseGdtf();
  const std::string hexColor = "#22AA66";
  assert(SetGdtfModelColor(gdtfPath, hexColor));

  wxFileInputStream input(gdtfPath);
  assert(input.IsOk());
  wxZipInputStream zipInput(input);

  std::string descriptionXml;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    if (entry->GetName().ToStdString() == "description.xml") {
      descriptionXml = ReadCurrentZipEntry(zipInput);
      break;
    }
  }

  assert(!descriptionXml.empty());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionXml.c_str(), descriptionXml.size()) ==
         tinyxml2::XML_SUCCESS);

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);

  const char *editor = fixtureType->Attribute("Editor");
  assert(editor == nullptr);

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  assert(models != nullptr);
  const std::string expectedCie = HexToCieForAssertion(hexColor);
  assert(!expectedCie.empty());
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *color = model->Attribute("Color");
    assert(color != nullptr);
    assert(std::string(color) == expectedCie);
  }

  tinyxml2::XMLElement *audit = fixtureType->FirstChildElement("PerastageMutationAudit");
  assert(audit != nullptr);
  assert(audit->IntAttribute("SchemaVersion") ==
         GdtfMutationAudit::kPerastageGdtfMutationSchemaVersion);
  const char *auditVersion = audit->Attribute("PerastageVersion");
  assert(auditVersion != nullptr);
  assert(std::string(auditVersion) == app::kVersion);

  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  assert(revisions != nullptr);
  tinyxml2::XMLElement *revision = revisions->FirstChildElement("Revision");
  assert(revision != nullptr);

  const char *text = revision->Attribute("Text");
  const char *modifiedBy = revision->Attribute("ModifiedBy");
  const char *date = revision->Attribute("Date");
  assert(text != nullptr);
  assert(modifiedBy != nullptr);
  assert(date != nullptr && std::string(date).size() > 0);
  assert(std::string(text) == "Updated model color");
  assert(std::string(modifiedBy) == std::string("Perastage ") + app::kVersion);

  std::error_code ec;
  fs::remove(gdtfPath, ec);
  return 0;
}
