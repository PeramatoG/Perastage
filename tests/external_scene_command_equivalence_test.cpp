#include "command/command_text_processor.h"
#include "external_scene_workflow.h"
#include "mvr_import_package.h"
#include "mvr_read_service.h"
#include "mvrscene.h"
#include "support/gdtf_test_fixture_builder.h"

#include <cassert>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;
using namespace perastage;

namespace {

class RecordingHost final : public command::ProjectMutationHost {
public:
  // Records successful semantic publication without application Undo state.
  command::MutationPublication CommitMutation(
      const MvrScene &, const scene_grouping::ObjectSelection &,
      const std::string &) override {
    return {.undoEntryRecorded = false, .projectDirty = true};
  }
};

// Reads an entire binary fixture into memory.
std::string ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// Adds one byte payload to an open ZIP archive.
void AddEntry(wxZipOutputStream &archive, const std::string &name,
              const std::string &bytes) {
  archive.PutNextEntry(name);
  archive.Write(bytes.data(), bytes.size());
  archive.CloseEntry();
}

// Creates a deterministic single-fixture MVR for external workflow execution.
void WriteSourceMvr(const fs::path &path, const fs::path &gdtfPath) {
  static constexpr const char *sceneXml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<GeneralSceneDescription verMajor="1" verMinor="6"><UserData/><Scene><Layers>
<Layer uuid="11111111-1111-4111-8111-111111111111"><Name>Layer</Name><ChildList>
<Fixture uuid="22222222-2222-4222-8222-222222222222"><Name>Fixture</Name>
<Matrix>{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}</Matrix>
<FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>
<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode></Fixture>
</ChildList></Layer></Layers></Scene></GeneralSceneDescription>)xml";
  wxFFileOutputStream output(path.wstring());
  wxZipOutputStream archive(output);
  AddEntry(archive, "GeneralSceneDescription.xml", sceneXml);
  AddEntry(archive, "fixture.gdtf", ReadBytes(gdtfPath));
  archive.Close();
}

// Compares the stable translation and basis values of two transforms.
bool EquivalentTransform(const Matrix &left, const Matrix &right) {
  constexpr float tolerance = 0.01f;
  const std::array<std::array<float, 3>, 4> leftRows = {
      left.u, left.v, left.w, left.o};
  const std::array<std::array<float, 3>, 4> rightRows = {
      right.u, right.v, right.w, right.o};
  for (std::size_t row = 0; row < leftRows.size(); ++row)
    for (std::size_t column = 0; column < leftRows[row].size(); ++column)
      if (std::fabs(leftRows[row][column] - rightRows[row][column]) >
          tolerance)
        return false;
  return true;
}

} // namespace

// Verifies shared and external workflows produce equivalent semantic transforms.
int main() {
  const fs::path root = fs::temp_directory_path() /
                        "perastage-external-command-equivalence-test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root);
  const fs::path gdtfPath = root / "fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Fixture", "Test",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WriteArchive(gdtfPath);
  const fs::path inputPath = root / "input.mvr";
  const fs::path outputPath = root / "output.mvr";
  WriteSourceMvr(inputPath, gdtfPath);

  MvrScene directScene;
  Fixture fixture;
  fixture.uuid = "22222222-2222-4222-8222-222222222222";
  fixture.fixtureId = 1;
  directScene.fixtures.emplace(fixture.uuid, fixture);
  scene_grouping::ObjectSelection selection;
  RecordingHost host;
  command::ExecutionContext context{directScene, selection, host};
  const scene_grouping::InteractiveTransformPolicy policy;
  const std::string commands = "f 1 pos x 1 pos y ++2 rot z 30";
  const command::text::LineExecutionResult direct =
      command::text::ProcessCommandLine(commands, context, policy);
  assert(direct.Success() && direct.records.size() == 4);
  assert(direct.mutation.selectionChanged && direct.mutation.sceneChanged);

  external_scene::Request request;
  request.inputPath = inputPath;
  request.outputPath = outputPath;
  request.commands = {commands};
  const external_scene::Result external = external_scene::Execute(request);
  assert(external.success && external.commandResults.size() == 4);
  assert(external.selectionChanged && external.sceneChanged);
  for (std::size_t index = 0; index < direct.records.size(); ++index) {
    assert(external.commandResults[index].request->commandId ==
           direct.records[index].result.request->commandId);
    assert(external.commandResults[index].mutation.sceneChanged ==
           direct.records[index].result.mutation.sceneChanged);
    assert(external.commandResults[index].mutation.selectionChanged ==
           direct.records[index].result.mutation.selectionChanged);
  }

  MvrImportResult imported;
  auto package = mvr::AcquireImportPackage(outputPath, imported.diagnostics, false);
  assert(package);
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.preserveMvrGdtfReferences = true;
  options.allowDummyFallback = false;
  assert(mvr::ReadAcquiredMvrPackage(*package, imported, options));
  assert(EquivalentTransform(imported.scene.fixtures.at(fixture.uuid).transform,
                             directScene.fixtures.at(fixture.uuid).transform));
  fs::remove_all(root, error);
  return 0;
}
