#include "external_scene_workflow.h"

#include "command/command_execution.h"
#include "command/command_text_processor.h"
#include "mvr_import_package.h"
#include "mvr_read_service.h"
#include "mvrexporter.h"

#include <chrono>
#include <fstream>
#include <system_error>

namespace perastage::external_scene {
namespace fs = std::filesystem;
namespace {

class HeadlessMutationHost final : public command::ProjectMutationHost {
public:
  // Marks a successful headless semantic mutation as dirty without GUI Undo.
  command::MutationPublication CommitMutation(
      const MvrScene &, const scene_grouping::ObjectSelection &,
      const std::string &) override {
    dirty_ = true;
    return {.undoEntryRecorded = false, .projectDirty = true};
  }

  // Reports whether any semantic Command published a headless mutation.
  bool dirty() const { return dirty_; }

private:
  bool dirty_ = false;
};

// Resolves enough path spelling and symlink aliases to protect input ownership.
fs::path ComparablePath(const fs::path &path) {
  std::error_code error;
  fs::path resolved = fs::weakly_canonical(path, error);
  if (!error)
    return resolved;
  error.clear();
  return fs::absolute(path, error).lexically_normal();
}

// Writes all serialized bytes before replacing any requested destination.
bool Publish(const fs::path &output, const std::vector<std::uint8_t> &bytes,
             bool overwrite, std::string &diagnostic) {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path staged = output.string() + ".perastage-stage-" +
                          std::to_string(nonce);
  const fs::path backup = output.string() + ".perastage-backup-" +
                          std::to_string(nonce);
  {
    std::ofstream stream(staged, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(reinterpret_cast<const char *>(bytes.data()),
                                 static_cast<std::streamsize>(bytes.size()))) {
      diagnostic = "Could not write the staged output MVR.";
      std::error_code ignored;
      fs::remove(staged, ignored);
      return false;
    }
  }

  std::error_code error;
  const bool existed = fs::exists(output, error) && !error;
  if (existed && overwrite) {
    fs::rename(output, backup, error);
    if (error) {
      fs::remove(staged, error);
      diagnostic = "Could not stage the existing output for replacement.";
      return false;
    }
  }
  fs::rename(staged, output, error);
  if (error) {
    if (existed && overwrite) {
      std::error_code restoreError;
      fs::rename(backup, output, restoreError);
    }
    std::error_code ignored;
    fs::remove(staged, ignored);
    diagnostic = "Could not publish the output MVR.";
    return false;
  }
  if (existed && overwrite)
    fs::remove(backup, error);
  return true;
}

} // namespace

// Runs an isolated MVR read, semantic Command sequence, and canonical publish.
Result Execute(const Request &request) {
  Result result;
  if (request.inputPath.extension() != ".mvr") {
    result.diagnostics.push_back("Mutation input must be an .mvr file.");
    return result;
  }
  if (request.outputPath.empty()) {
    result.diagnostics.push_back("An explicit output MVR is required.");
    return result;
  }
  if (ComparablePath(request.inputPath) == ComparablePath(request.outputPath)) {
    result.diagnostics.push_back("Input and output must be different files.");
    return result;
  }
  std::error_code error;
  if (fs::exists(request.outputPath, error) && !request.overwrite) {
    result.diagnostics.push_back(
        "Output already exists; use --overwrite to replace it.");
    return result;
  }

  MvrImportResult imported;
  auto package = mvr::AcquireImportPackage(request.inputPath,
                                           imported.diagnostics, false);
  if (!package) {
    result.diagnostics.push_back("Could not acquire the input MVR package.");
    return result;
  }
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.preserveMvrGdtfReferences = true;
  options.allowDummyFallback = false;
  if (!mvr::ReadAcquiredMvrPackage(*package, imported, options)) {
    result.diagnostics.push_back("Could not read the input MVR scene.");
    return result;
  }

  scene_grouping::ObjectSelection selection;
  HeadlessMutationHost host;
  command::ExecutionContext context{imported.scene, selection, host};
  const scene_grouping::InteractiveTransformPolicy transformPolicy;
  for (const std::string &text : request.commands) {
    command::text::LineExecutionResult execution =
        command::text::ProcessCommandLine(text, context, transformPolicy);
    for (auto &record : execution.records)
      result.commandResults.push_back(std::move(record.result));
    if (!execution.Success()) {
      for (const command::Diagnostic &diagnostic : execution.parseDiagnostics)
        result.diagnostics.push_back(diagnostic.message);
      if (result.diagnostics.empty())
        result.diagnostics.push_back("A semantic command failed.");
      return result;
    }
  }
  result.sceneChanged = host.dirty();

  std::vector<std::uint8_t> archive;
  MvrExporter exporter;
  if (!exporter.ExportCanonicalSnapshotToBuffer(imported.scene, archive)) {
    result.diagnostics.push_back("Could not serialize the canonical output MVR.");
    return result;
  }
  std::string publicationDiagnostic;
  if (!Publish(request.outputPath, archive, request.overwrite,
               publicationDiagnostic)) {
    result.diagnostics.push_back(std::move(publicationDiagnostic));
    return result;
  }
  result.outputPublished = true;
  result.success = true;
  return result;
}

} // namespace perastage::external_scene
