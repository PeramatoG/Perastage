#include "configmanager.h"
#include "LayoutManager.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvr_merge_analyzer.h"
#include "mvr_merge_applier.h"
#include "mvr_standard_preservation.h"
#include "mvr_source_structure_service.h"
#include "mvr_model_dependencies.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include "support/mvr_structure_fixture.h"
#include "inspection/xml_schema_validation.h"
#include <tinyxml2.h>
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <wx/init.h>
#include <wx/mstream.h>

namespace fs = std::filesystem;
static std::string Read(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
static void Write(const fs::path &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary);
  out << bytes;
}
int main(int argc, char **argv) {
  wxInitializer initialize;
  assert(initialize.IsOk());
  const std::string mode = argc > 1 ? argv[1] : "";
  const bool structured = mode.starts_with("--structure");
  const bool geometryEdit = mode.find("geometry") != std::string::npos;
  const bool dependencies = mode.find("dependencies") != std::string::npos;
  const bool merge = mode.find("merge") != std::string::npos;
  const auto dir = fs::temp_directory_path() /
                   ("perastage-preserved-fields-roundtrip" + mode);
  fs::remove_all(dir);
  fs::create_directories(dir);
  const auto gdtf = dir / "fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithDmxMode("Mode", "Base")
      .WriteArchive(gdtf);
  const std::string id = "51000000-0000-4000-8000-000000000001";
  const std::string ordinaryXml =
      "<GeneralSceneDescription verMajor='1' verMinor='6' provider='Test' "
      "providerVersion='1'>"
      "<Scene><Layers><Layer uuid='11000000-0000-4000-8000-000000000001' "
      "name='Stage'><ChildList>"
      "<Fixture uuid='" +
      id +
      "' name='Original'><Matrix>{1,0,0}{0,1,0}{0,0,1}{100,200,300}</Matrix>"
      "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>"
      "<CastShadow>true</CastShadow><FixtureID>1</FixtureID><UnitNumber>1</"
      "UnitNumber><CustomCommands><CustomCommand>Pan,f "
      "50</CustomCommand></CustomCommands>"
      "</Fixture></ChildList></Layer></Layers></Scene></"
      "GeneralSceneDescription>";
  const std::string xml =
      structured ? tests::mvr_structure::Xml(id) : ordinaryXml;
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(
      xml, perastage::inspection::Mvr16Schema());
  if (validation.schema.status !=
      perastage::inspection::ValidationStatus::Valid)
    for (const auto &diagnostic : validation.schema.diagnostics)
      std::cerr << diagnostic.message << "\n";
  assert(validation.schema.status ==
         perastage::inspection::ValidationStatus::Valid);
  const auto source = dir / "source.mvr";
  {
    wxFFileOutputStream out(source.string());
    wxZipOutputStream zip(out);
    assert(zip.PutNextEntry("GeneralSceneDescription.xml"));
    zip.Write(xml.data(), xml.size());
    assert(zip.CloseEntry());
    const auto bytes = Read(gdtf);
    assert(zip.PutNextEntry("fixture.gdtf"));
    zip.Write(bytes.data(), bytes.size());
    assert(zip.CloseEntry());
    if (structured) {
      const auto mesh = tests::mvr_structure::Mesh(dependencies);
      assert(zip.PutNextEntry("mesh.3ds"));
      zip.Write(mesh.data(), mesh.size());
      assert(zip.CloseEntry());
      if (dependencies) {
        const auto texture = tests::mvr_structure::Texture();
        assert(zip.PutNextEntry("textures/mask.png"));
        zip.Write(texture.data(), texture.size());
        assert(zip.CloseEntry());
      }
      const std::string media = "synthetic media evidence";
      assert(zip.PutNextEntry("movie.bin"));
      zip.Write(media.data(), media.size());
      assert(zip.CloseEntry());
    }
    assert(zip.Close());
  }
  const auto original = Read(source);
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  assert(MvrImporter::ImportAndRegister(source.string(), options));
  auto &fixture = cfg.GetScene().fixtures.at(id);
  fixture.instanceName = "Edited fixture";
  fixture.transform.o[0] += 1.25f;
  const float editedX = fixture.transform.o[0];
  layouts::LayoutDefinition layout;
  layout.name = "Edited layout";
  layouts::LayoutTextDefinition note;
  note.id = 1;
  note.text = "Session changes survive";
  note.frame = {1, 2, 100, 50};
  layout.textViews.push_back(note);
  assert(layouts::LayoutManager::Get().AddLayout(layout));
  std::map<std::string, Matrix> editedWorlds;
  if (structured) {
    auto &child = cfg.GetScene().fixtures.at(tests::mvr_structure::child);
    child.instanceName = "Edited child";
    child.transform.o[1] += 2.5f;
    for (const auto &[uuid, value] : cfg.GetScene().sceneObjects)
      editedWorlds[uuid] = value.transform;
    for (const auto &[uuid, value] : cfg.GetScene().supports)
      editedWorlds[uuid] = value.transform;
    for (auto &[uuid, value] : cfg.GetScene().trusses) {
      value.transform.o[0] += 4.25f;
      editedWorlds[uuid] = value.transform;
    }
    editedWorlds[child.uuid] = child.transform;
  }
  std::optional<Matrix> editedGeometry;
  const auto focusGeometry = [&]() -> GeometryInstance & {
    auto &geometries =
        cfg.GetScene().sceneObjects.at(tests::mvr_structure::focus).geometries;
    const auto found = std::find_if(
        geometries.begin(), geometries.end(), [](const auto &value) {
          return value.sourceSymbolUuid == tests::mvr_structure::focusSymbol;
        });
    assert(found != geometries.end());
    return *found;
  };
  if (geometryEdit) {
    auto &geometry = focusGeometry();
    auto originalModel = fs::path(geometry.modelFile);
    if (!originalModel.is_absolute())
      originalModel = fs::path(cfg.GetScene().basePath) / originalModel;
    const auto editedModel = originalModel.parent_path() / "edited-mesh.3ds";
    Write(editedModel, Read(originalModel));
    geometry.modelFile = editedModel.string();
    geometry.localTransform.o[1] += 2.5f;
    editedGeometry = geometry.localTransform;
  }
  std::string mergedFixture;
  if (merge) {
    MvrImporter reader;
    MvrScene incoming;
    assert(reader.ImportSceneFromFile(source.string(), incoming, options));
    const auto analysis =
        mvr::AnalyzeImportedSceneMerge(cfg.GetScene(), incoming);
    mergedFixture = mvr::RemapImportedUuidReference(id, analysis);
    assert(mergedFixture != id);
    auto result =
        mvr::ApplyImportedSceneMerge(cfg.GetScene(), incoming, analysis);
    assert(result.fixtureTypeConflictsBlocked == 0 &&
           cfg.GetScene().fixtures.size() == 4);
  }
  const auto target = dir / "project.pstg";
  // Exercise real project history with immutable source provenance attached.
  cfg.PushUndoState("Temporary fixture edit");
  cfg.GetScene().fixtures.at(id).instanceName = "Temporary";
  assert(cfg.CanUndo());
  cfg.Undo();
  assert(cfg.GetScene().fixtures.at(id).instanceName == "Edited fixture");
  assert(cfg.CanRedo());
  cfg.Redo();
  assert(cfg.GetScene().fixtures.at(id).instanceName == "Temporary");
  cfg.Undo();
  assert(cfg.GetScene().fixtures.at(id).instanceName == "Edited fixture");
  Write(target, "previous target");
  const bool saved = cfg.SaveProject(target.string());
  if (argc > 1 && std::string(argv[1]) == "--baseline") {
    assert(!saved && cfg.GetLastProjectSaveFailure().kind ==
                         ProjectSaveFailureKind::MvrPreservation);
    assert(Read(target) == "previous target" && Read(source) == original);
    std::cout << cfg.GetLastProjectSaveFailure().detail << '\n';
    cfg.Reset();
    fs::remove_all(dir);
    return 0;
  }
  assert(saved && Read(source) == original);
  const auto standalone = dir / "canonical.mvr";
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(cfg.GetScene(),
                                                standalone.string()));
  std::string embedded;
  assert(tests::zip::ReadEntry(target, "scene.mvr", &embedded));
  Write(dir / "embedded.mvr", embedded);
  const auto standaloneEntries = tests::zip::ReadEntries(standalone);
  const auto embeddedEntries = tests::zip::ReadEntries(dir / "embedded.mvr");
  assert(standaloneEntries.size() == embeddedEntries.size());
  for (std::size_t i = 0; i < standaloneEntries.size(); ++i) {
    assert(standaloneEntries[i].name == embeddedEntries[i].name);
    assert(standaloneEntries[i].payload == embeddedEntries[i].payload);
  }
  std::string exportedXml;
  assert(tests::zip::ReadEntry(standalone, "GeneralSceneDescription.xml",
                               &exportedXml));
  tinyxml2::XMLDocument document;
  assert(document.Parse(exportedXml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *exportedFixture = nullptr;
  std::vector<tinyxml2::XMLElement *> pending{document.RootElement()};
  while (!pending.empty()) {
    auto *node = pending.back();
    pending.pop_back();
    if (std::string(node->Name()) == "Fixture" && node->Attribute("uuid") &&
        id == node->Attribute("uuid"))
      exportedFixture = node;
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      pending.push_back(child);
  }
  assert(exportedFixture &&
         std::string(
             exportedFixture->FirstChildElement("CastShadow")->GetText()) ==
             "true");
  assert(std::string(exportedFixture->FirstChildElement("CustomCommands")
                         ->FirstChildElement("CustomCommand")
                         ->GetText()) == "Pan,f 50");
  if (structured) {
    const auto find = [&](const std::string &uuid) {
      std::vector<tinyxml2::XMLElement *> nodes{document.RootElement()};
      while (!nodes.empty()) {
        auto *node = nodes.back();
        nodes.pop_back();
        if (node->Attribute("uuid") && uuid == node->Attribute("uuid") &&
            std::string(node->Name()) != "Entry")
          return node;
        for (auto *child = node->FirstChildElement(); child;
             child = child->NextSiblingElement())
          if (std::string(child->Name()) != "UserData")
            nodes.push_back(child);
      }
      return static_cast<tinyxml2::XMLElement *>(nullptr);
    };
    using namespace tests::mvr_structure;
    assert(find(video) && find(projector) && find(innerSymbol) &&
           find(focusSymbol) && find(secondFocusSymbol));
    assert(std::string(find(secondFocusSymbol)->Attribute("symdef")) ==
           definition);
    assert(std::string(find(focus)->Parent()->Parent()->ToElement()->Attribute(
               "uuid")) == id);
    assert(std::string(find(child)->Parent()->Parent()->ToElement()->Attribute(
               "uuid")) == projector);
    assert(std::string(find(child)->Attribute("multipatch")) == id);
    if (geometryEdit) {
      assert(std::string(find(focusSymbol)->Attribute("symdef")) != nested);
      assert(find(definition) && find(nested));
    } else
      assert(std::string(find(focusSymbol)->Attribute("symdef")) == nested);
    assert(std::string(
               find(video)->FirstChildElement("FixtureIDNumeric")->GetText()) ==
           "4294967296");
    if (merge) {
      assert(find(mergedFixture));
      assert(std::string(find(mergedFixture)->Attribute("name")) == "Original");
    }
    if (dependencies) {
      auto *geometry = find(definition)
                           ->FirstChildElement("ChildList")
                           ->FirstChildElement("Geometry3D");
      const std::string name = geometry->Attribute("fileName");
      std::string bytes;
      assert(tests::zip::ReadEntry(standalone, name, &bytes));
      assert(bytes == tests::mvr_structure::Mesh(true));
      const auto texturePath =
          (fs::path(name).parent_path() / "textures/mask.png").generic_string();
      assert(tests::zip::ReadEntry(standalone, texturePath, &bytes));
      assert(bytes == tests::mvr_structure::Texture());
    }
    std::string media;
    assert(tests::zip::ReadEntry(standalone, "movie.bin", &media));
    assert(media == "synthetic media evidence");
    // Perastage provider metadata is validated separately by its existing
    // owner; validate the generated standard Scene against the pinned standard
    // type.
    auto *userData = document.RootElement()->FirstChildElement("UserData");
    if (userData)
      document.RootElement()->DeleteChild(userData);
    tinyxml2::XMLPrinter printer;
    document.Print(&printer);
    const auto standard = perastage::inspection::ValidateXmlAgainstSchema(
        printer.CStr(), perastage::inspection::Mvr16Schema());
    assert(standard.schema.status ==
           perastage::inspection::ValidationStatus::Valid);
    MvrExportOptions compatibility;
    compatibility.trussGeometryExportMode =
        MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
    const auto direct = dir / "compatibility.mvr";
    assert(exporter.ExportCanonicalSnapshotToFile(
        cfg.GetScene(), direct.string(), compatibility));
    std::string directXml;
    assert(tests::zip::ReadEntry(direct, "GeneralSceneDescription.xml",
                                 &directXml));
    tinyxml2::XMLDocument directDocument;
    assert(directDocument.Parse(directXml.c_str()) == tinyxml2::XML_SUCCESS);
    auto *layers = directDocument.RootElement()
                       ->FirstChildElement("Scene")
                       ->FirstChildElement("Layers");
    auto *rigLayer = layers->FirstChildElement("Layer");
    while (rigLayer && std::string(rigLayer->Attribute("uuid")) != layer)
      rigLayer = rigLayer->NextSiblingElement("Layer");
    assert(rigLayer);
    auto *rig =
        rigLayer->FirstChildElement("ChildList")->FirstChildElement("Truss");
    assert(
        rig &&
        rig->FirstChildElement("Geometries")->FirstChildElement("Geometry3D"));
    assert(!rig->FirstChildElement("Geometries")->FirstChildElement("Symbol"));
  }
  cfg.Reset();
  assert(cfg.LoadProject(target.string()));
  const auto &restored = cfg.GetScene().fixtures.at(id);
  assert(restored.instanceName == "Edited fixture" &&
         restored.transform.o[0] == editedX);
  bool layoutFound = false;
  for (const auto &page : layouts::LayoutManager::Get().GetLayouts().Items())
    if (page.name == layout.name)
      layoutFound = page.textViews[0].text == note.text;
  assert(layoutFound && Read(source) == original);
  if (structured)
    for (const auto &[uuid, expected] : editedWorlds) {
      Matrix actual;
      if (cfg.GetScene().fixtures.contains(uuid))
        actual = cfg.GetScene().fixtures.at(uuid).transform;
      else if (cfg.GetScene().sceneObjects.contains(uuid))
        actual = cfg.GetScene().sceneObjects.at(uuid).transform;
      else if (cfg.GetScene().supports.contains(uuid))
        actual = cfg.GetScene().supports.at(uuid).transform;
      else
        actual = cfg.GetScene().trusses.at(uuid).transform;
      assert(actual.u == expected.u && actual.v == expected.v &&
             actual.w == expected.w && actual.o == expected.o);
    }

  if (editedGeometry) {
    const auto &restoredGeometry = focusGeometry();
    assert(restoredGeometry.localTransform.o == editedGeometry->o);
    assert(fs::path(restoredGeometry.modelFile).filename() ==
           "edited-mesh.3ds");
  }
  if (merge)
    assert(cfg.GetScene().fixtures.at(mergedFixture).instanceName ==
           "Original");
  assert(cfg.SaveProject((dir / "second.pstg").string()));
  const auto definitions = cfg.GetScene().symdefs.size();
  assert(cfg.LoadProject((dir / "second.pstg").string()));
  assert(cfg.GetScene().symdefs.size() == definitions);
  assert(cfg.SaveProject((dir / "third.pstg").string()));
  if (structured) {
    const auto before = Read(target);
    cfg.MarkDirty();
    if (dependencies) {
      auto model = fs::path(cfg.GetScene()
                                .symdefs.at(tests::mvr_structure::definition)
                                .geometries.at(0)
                                .file);
      if (!model.is_absolute())
        model = fs::path(cfg.GetScene().basePath) / model;
      const auto texture = model.parent_path() / "textures/mask.png";
      fs::rename(texture, texture.string() + ".held");
      assert(!cfg.SaveProject(target.string()));
      assert(cfg.GetLastProjectSaveFailure().kind ==
             ProjectSaveFailureKind::Resource);
      assert(Read(target) == before && cfg.IsDirty());
      fs::rename(texture.string() + ".held", texture);
    }
    const auto media = fs::path(cfg.GetScene().basePath) / "movie.bin";
    fs::rename(media, media.string() + ".held");
    fs::create_directories(media.parent_path() / "other");
    Write(media.parent_path() / "other" / "movie.bin", "unrelated namesake");
    assert(!cfg.SaveProject(target.string()));
    assert(cfg.GetLastProjectSaveFailure().kind ==
           ProjectSaveFailureKind::Resource);
    assert(Read(target) == before && cfg.IsDirty() &&
           !fs::exists(target.string() + ".tmp"));
    fs::rename(media.string() + ".held", media);
    auto broken = cfg.GetScene();
    broken.symdefs.at(tests::mvr_structure::nested).symbols[0].symdef =
        tests::mvr_structure::nested;
    std::vector<uint8_t> blocked;
    assert(!exporter.ExportCanonicalSnapshotToBuffer(broken, blocked));
    broken = cfg.GetScene();
    auto invalid =
        std::make_shared<MvrSourceStructure>(*broken.sourceStructures.front());
    for (auto &node : invalid->nodes)
      if (node.kind == "VideoScreen")
        node.uuid = "00000000-0000-0000-0000-000000000000";
    mvr::IndexSourceStructure(*invalid);
    broken.sourceStructures.front() = invalid;
    assert(!exporter.ExportCanonicalSnapshotToBuffer(broken, blocked));
    assert(std::any_of(
        exporter.GetExportDiagnostics().begin(),
        exporter.GetExportDiagnostics().end(), [](const auto &item) {
          return item.code == MvrExportDiagnosticCode::IdentityConflict;
        }));
  }

  // A malformed retained field is distinct from a valid standard field. It
  // cannot become a successful, silently reduced-fidelity save.
  const auto goodScene = cfg.GetScene();
  for (auto &issue : cfg.GetScene().preservationIssues) {
    if (issue.node == "CastShadow")
      issue.originalInformation = "<CastShadow>invalid</CastShadow>";
    if (issue.node == "CustomCommands")
      issue.originalInformation = "<CustomCommands><Invalid/></CustomCommands>";
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
  assert(cfg.GetLastProjectSaveFailure().kind ==
         ProjectSaveFailureKind::ArchiveIo);
  assert(cfg.IsDirty() && Read(target) == previous);
  cfg.GetScene().fixtures.at(id).gdtfSpec =
      (dir / "missing-authored.gdtf").string();
  assert(!cfg.SaveProject(target.string()));
  assert(cfg.GetLastProjectSaveFailure().kind ==
         ProjectSaveFailureKind::Resource);
  assert(Read(target) == previous && cfg.IsDirty());
  cfg.GetScene() = goodScene;
  const auto directoryTarget = dir / "directory.pstg";
  fs::create_directory(directoryTarget);
  Write(directoryTarget / "keep", "preserved");
  assert(!cfg.SaveProject(directoryTarget.string()));
  assert(cfg.GetLastProjectSaveFailure().kind ==
         ProjectSaveFailureKind::ArchiveIo);
  assert(Read(directoryTarget / "keep") == "preserved" &&
         !fs::exists(directoryTarget.string() + ".tmp"));
  cfg.Reset();
  fs::remove_all(dir);
}
