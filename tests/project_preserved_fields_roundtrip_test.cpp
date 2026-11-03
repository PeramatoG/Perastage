#include "configmanager.h"
#include "LayoutManager.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include "inspection/xml_schema_validation.h"
#include <tinyxml2.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <wx/init.h>
#include <wx/mstream.h>

namespace fs = std::filesystem;
static std::string Read(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
static void Write(const fs::path &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary); out << bytes;
}
int main(int argc, char **argv) {
  wxInitializer initialize; assert(initialize.IsOk());
  const auto dir = fs::temp_directory_path() / "perastage-preserved-fields-roundtrip";
  fs::remove_all(dir); fs::create_directories(dir);
  const auto gdtf = dir / "fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Base").WriteArchive(gdtf);
  const std::string id = "51000000-0000-4000-8000-000000000001";
  const std::string xml = "<GeneralSceneDescription verMajor='1' verMinor='6' provider='Test' providerVersion='1'>"
      "<Scene><Layers><Layer uuid='11000000-0000-4000-8000-000000000001' name='Stage'><ChildList>"
      "<Fixture uuid='" + id + "' name='Original'><Matrix>{1,0,0}{0,1,0}{0,0,1}{100,200,300}</Matrix>"
      "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>"
      "<CastShadow>true</CastShadow><FixtureID>1</FixtureID><UnitNumber>1</UnitNumber><CustomCommands><CustomCommand>Pan,f 50</CustomCommand></CustomCommands>"
      "</Fixture></ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(xml, perastage::inspection::Mvr16Schema());
  assert(validation.schema.status == perastage::inspection::ValidationStatus::Valid);
  const auto source = dir / "source.mvr";
  { wxFFileOutputStream out(source.string()); wxZipOutputStream zip(out);
    assert(zip.PutNextEntry("GeneralSceneDescription.xml")); zip.Write(xml.data(), xml.size()); assert(zip.CloseEntry());
    const auto bytes = Read(gdtf); assert(zip.PutNextEntry("fixture.gdtf")); zip.Write(bytes.data(),bytes.size());
    assert(zip.CloseEntry()); assert(zip.Close()); }
  const auto original = Read(source);
  auto &cfg = ConfigManager::Get(); cfg.Reset();
  MvrImportOptions options; options.promptConflicts = false; options.applyDictionary = false; options.allowDummyFallback = false;
  assert(MvrImporter::ImportAndRegister(source.string(), options));
  auto &fixture = cfg.GetScene().fixtures.at(id);
  fixture.instanceName = "Edited fixture"; fixture.transform.o[0] += 1.25f;
  const float editedX = fixture.transform.o[0];
  layouts::LayoutDefinition layout; layout.name = "Edited layout";
  layouts::LayoutTextDefinition note; note.id = 1; note.text = "Session changes survive"; note.frame={1,2,100,50};
  layout.textViews.push_back(note); assert(layouts::LayoutManager::Get().AddLayout(layout));
  const auto target = dir / "project.pstg";
  Write(target, "previous target");
  const bool saved = cfg.SaveProject(target.string());
  if (argc > 1 && std::string(argv[1]) == "--baseline") {
    assert(!saved && cfg.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::MvrPreservation);
    assert(Read(target) == "previous target" && Read(source) == original);
    std::cout << cfg.GetLastProjectSaveFailure().detail << '\n';
    cfg.Reset(); fs::remove_all(dir); return 0;
  }
  assert(saved && Read(source) == original);
  const auto standalone = dir / "canonical.mvr";
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(cfg.GetScene(), standalone.string()));
  std::string embedded;
  assert(tests::zip::ReadEntry(target, "scene.mvr", &embedded));
  Write(dir / "embedded.mvr", embedded);
  const auto standaloneEntries = tests::zip::ReadEntries(standalone);
  const auto embeddedEntries = tests::zip::ReadEntries(dir / "embedded.mvr");
  assert(standaloneEntries.size() == embeddedEntries.size());
  for (std::size_t i=0; i<standaloneEntries.size(); ++i) {
    assert(standaloneEntries[i].name == embeddedEntries[i].name);
    assert(standaloneEntries[i].payload == embeddedEntries[i].payload);
  }
  std::string exportedXml;
  assert(tests::zip::ReadEntry(standalone, "GeneralSceneDescription.xml", &exportedXml));
  tinyxml2::XMLDocument document;
  assert(document.Parse(exportedXml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *exportedFixture = nullptr;
  std::vector<tinyxml2::XMLElement *> pending{document.RootElement()};
  while (!pending.empty()) {
    auto *node = pending.back(); pending.pop_back();
    if (std::string(node->Name()) == "Fixture" && node->Attribute("uuid") && id == node->Attribute("uuid")) exportedFixture = node;
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement()) pending.push_back(child);
  }
  assert(exportedFixture && std::string(exportedFixture->FirstChildElement("CastShadow")->GetText()) == "true");
  assert(std::string(exportedFixture->FirstChildElement("CustomCommands")->FirstChildElement("CustomCommand")->GetText()) == "Pan,f 50");
  cfg.Reset(); assert(cfg.LoadProject(target.string()));
  const auto &restored = cfg.GetScene().fixtures.at(id);
  assert(restored.instanceName == "Edited fixture" && restored.transform.o[0] == editedX);
  bool layoutFound = false;
  for (const auto &page : layouts::LayoutManager::Get().GetLayouts().Items())
    if (page.name == layout.name) layoutFound = page.textViews[0].text == note.text;
  assert(layoutFound && Read(source) == original);
  assert(cfg.SaveProject((dir / "second.pstg").string()));
  // A malformed retained field is distinct from a valid standard field. It
  // cannot become a successful, silently reduced-fidelity save.
  const auto goodScene = cfg.GetScene();
  for (auto &issue : cfg.GetScene().preservationIssues) {
    if (issue.node == "CastShadow") issue.originalInformation = "<CastShadow>invalid</CastShadow>";
    if (issue.node == "CustomCommands") issue.originalInformation = "<CustomCommands><Invalid/></CustomCommands>";
  }
  cfg.MarkDirty();
  const auto previous = Read(target);
  assert(!cfg.SaveProject(target.string()));
  const auto failure = cfg.GetLastProjectSaveFailure();
  assert(failure.kind == ProjectSaveFailureKind::MvrPreservation);
  assert(failure.detail.find("CastShadow") != std::string::npos);
  assert(failure.detail.find("CustomCommands") != std::string::npos);
  assert(failure.detail.find(id) != std::string::npos);
  assert(cfg.IsDirty() && Read(target) == previous && Read(source) == original);
  assert(!fs::exists(target.string() + ".tmp"));
  cfg.GetScene() = goodScene;
  assert(!cfg.SaveProject((dir / "absent" / "project.pstg").string()));
  assert(cfg.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::ArchiveIo);
  assert(cfg.IsDirty() && Read(target) == previous);
  cfg.GetScene().fixtures.at(id).gdtfSpec = (dir / "missing-authored.gdtf").string();
  assert(!cfg.SaveProject(target.string()));
  assert(cfg.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::Resource);
  assert(Read(target) == previous && cfg.IsDirty());
  cfg.GetScene() = goodScene;
  const auto directoryTarget = dir / "directory.pstg";
  fs::create_directory(directoryTarget); Write(directoryTarget / "keep", "preserved");
  assert(!cfg.SaveProject(directoryTarget.string()));
  assert(cfg.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::ArchiveIo);
  assert(Read(directoryTarget / "keep") == "preserved" && !fs::exists(directoryTarget.string() + ".tmp"));
  cfg.Reset(); fs::remove_all(dir);
}
