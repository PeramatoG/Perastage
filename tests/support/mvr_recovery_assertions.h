#pragma once
#include "mvrexporter.h"
#include "fixture_patch_service.h"
#include "fixture_network_service.h"
#include "mvr_identifier_preservation.h"
#include "inspection/xml_schema_validation.h"
#include "support/zip_test_utils.h"
#include <cassert>
#include <filesystem>
#include <tinyxml2.h>
namespace tests::recovery {
// Verify a real published archive and its standard scene, not just preflight.
inline std::string Export(MvrExporter &exporter, const MvrScene &scene,
                          const std::filesystem::path &path,
                          bool warning = true) {
  assert(exporter.ExportCanonicalSnapshotToFile(scene, path.string()));
  bool warned = false;
  for (const auto &finding : exporter.GetExportDiagnostics()) {
    assert(finding.severity != MvrExportDiagnosticSeverity::Error);
    warned |= finding.severity == MvrExportDiagnosticSeverity::Warning;
  }
  if (warning)
    assert(warned);
  std::string xml;
  assert(zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  auto *root = document.RootElement();
  assert(root);
  if (auto *data = root->FirstChildElement("UserData"))
    root->DeleteChild(data);
  // The pinned XSD disagrees with published MVR syntax for IPv6, signed
  // breaks, signed identifiers and Universe.Address. Qualify only those known
  // fields on a validation copy; actual output and independent
  // lexical/roundtrip checks stay intact.
  const auto qualify = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (std::string(node->Name()) == "Address") {
      if (auto *text = node->GetText();
          text && std::string(text).find('.') != std::string::npos) {
        assert(perastage::patch::IsDmxValue(text));
        node->SetText("1");
      }
      if (auto *raw = node->Attribute("break"); raw && raw[0] == '-') {
        assert(perastage::patch::DmxInteger(raw, true));
        node->SetAttribute("break", "0");
      }
    }
    if ((std::string(node->Name()) == "FixtureIDNumeric" ||
         std::string(node->Name()) == "UnitNumber") &&
        node->GetText() && node->GetText()[0] == '-') {
      assert(mvr_identifiers::IsIntegerLexical(node->GetText()));
      node->SetText("0");
    }
    if (std::string(node->Name()) == "Network")
      if (auto *value = node->Attribute("ipv6")) {
        assert(fixture_network::Ipv6(value));
        node->DeleteAttribute("ipv6");
      }
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, child);
  };
  qualify(qualify, root);
  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(
      printer.CStr(), perastage::inspection::Mvr16Schema());
  assert(validation.schema.status ==
         perastage::inspection::ValidationStatus::Valid);
  return xml;
}
inline size_t Count(const std::string &xml, const std::string &kind) {
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  size_t count = 0;
  const auto visit = [&](auto &&self,
                         const tinyxml2::XMLElement *node) -> void {
    if (std::string(node->Name()) == "UserData")
      return;
    if (std::string(node->Name()) == kind)
      ++count;
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, document.RootElement());
  return count;
}
} // namespace tests::recovery
