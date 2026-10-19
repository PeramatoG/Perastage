#include "configservices.h"
#include "startup_profile.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <wx/init.h>
#include <wx/log.h>

class UnexpectedWxLog : public wxLog {
public:
  bool receivedMessage = false;
protected:
  void DoLogText(const wxString &) override { receivedMessage = true; }
};

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  namespace fs = std::filesystem;

  const fs::path tempDir = fs::temp_directory_path() / "perastage_project_session_io_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir, ec);

  const fs::path projectPath = tempDir / "session_test.pera";
  ProjectSession saveSession;

  bool saveConfigCalled = false;
  bool saveSceneCalled = false;
  const bool saveOk = saveSession.SaveProject(
      projectPath.string(),
      [&](const std::string &path) {
        saveConfigCalled = true;
        std::ofstream out(path, std::ios::binary);
        out << "{\"mode\":\"test\"}";
        return out.good();
      },
      [&](const std::string &path) {
        saveSceneCalled = true;
        std::ofstream out(path, std::ios::binary);
        out << "PKSCENE";
        return out.good();
      });

  assert(saveOk);
  assert(saveConfigCalled);
  assert(saveSceneCalled);

  ProjectSession loadSession;
  bool loadConfigCalled = false;
  bool loadSceneCalled = false;
  const bool loadOk = loadSession.LoadProject(
      projectPath.string(),
      [&](const std::string &path) {
        loadConfigCalled = true;
        std::ifstream in(path, std::ios::binary);
        std::string body;
        std::getline(in, body, '\0');
        return body.find("mode") != std::string::npos;
      },
      [&](const std::string &path) {
        loadSceneCalled = true;
        std::ifstream in(path, std::ios::binary);
        std::string body;
        std::getline(in, body, '\0');
        return body == "PKSCENE";
      });

  assert(loadOk);
  assert(loadConfigCalled);
  assert(loadSceneCalled);

  auto memoryMetrics = std::make_shared<startup::Metrics>();
  ProjectSession memorySession;
  memorySession.SetStartupMetrics(memoryMetrics);
  bool receivedMemoryConfig = false;
  bool receivedMemoryScene = false;
  assert(memorySession.LoadProject(
      projectPath.string(),
      [&](const ProjectSession::ProjectConfigPayload &payload) {
        receivedMemoryConfig = payload.logicalName == "config.json" &&
                               !payload.bytes.empty();
        return receivedMemoryConfig;
      },
      [&](const ProjectSession::ProjectScenePayload &payload) {
        receivedMemoryScene = payload.IsMemoryBacked() &&
                              payload.bytes ==
                                  std::vector<std::uint8_t>({'P', 'K', 'S', 'C',
                                                             'E', 'N', 'E'});
        return receivedMemoryScene;
      }));
  assert(receivedMemoryConfig);
  assert(receivedMemoryScene);
  assert(memoryMetrics->sceneMvrMemoryRestores == 1);
  assert(memoryMetrics->sceneMvrTempFallbacks == 0);
  assert(memoryMetrics->configMemoryLoads == 1);

  auto fallbackMetrics = std::make_shared<startup::Metrics>();
  ProjectSession fallbackSession;
  fallbackSession.SetStartupMetrics(fallbackMetrics);
  assert(fallbackSession.LoadProject(
      projectPath.string(),
      [](const ProjectSession::ProjectConfigPayload &) { return true; },
      [](const ProjectSession::ProjectScenePayload &payload) {
        std::ifstream in(payload.spillPath, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        return !payload.IsMemoryBacked() && bytes == "PKSCENE";
      },
      {}, 2));
  assert(fallbackMetrics->sceneMvrTempFallbacks == 1);
  assert(fallbackMetrics->sceneMvrTempBytesWritten == 7);
  assert(memorySession.GetLoadedCacheValidationContext()
             .scenePackageFingerprint ==
         fallbackSession.GetLoadedCacheValidationContext()
             .scenePackageFingerprint);
  assert(memorySession.GetLoadedCacheValidationContext()
             .scenePackageFingerprint ==
         project_cache::FingerprintBytes("PKSCENE", 7));

  const fs::path fingerprintProjectPath =
      tempDir / "fingerprint_payload.pera";
  const std::vector<std::uint8_t> layoutImageBytes = {1, 2, 3, 4};
  assert(saveSession.SaveProject(
      fingerprintProjectPath.string(),
      [](std::vector<std::uint8_t> &out) {
        out = {'{', '}'};
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out = {'P', 'K', 'S'};
        return true;
      },
      ProjectSession::CollectArchiveResourcesFn([&layoutImageBytes] {
        return std::vector<ProjectSession::ArchiveResource>{{
            "resources/layout_images/example.rgba", layoutImageBytes}};
      })));
  ProjectSession fingerprintLoadSession;
  assert(fingerprintLoadSession.LoadProject(
      fingerprintProjectPath.string(),
      [](const ProjectSession::ProjectConfigPayload &) { return true; },
      [](const ProjectSession::ProjectScenePayload &) { return true; }));
  const auto &fingerprintContext =
      fingerprintLoadSession.GetLoadedCacheValidationContext();
  assert(fingerprintContext.HasCompletePackageCoverage());
  assert(fingerprintContext.scenePackageFingerprint ==
         project_cache::FingerprintBytes("PKS", 3));
  assert(fingerprintContext.packagedLayoutResourceFingerprint ==
         project_cache::AggregateNamedPayloadFingerprints({
             {"resources/layout_images/example.rgba",
              project_cache::FingerprintBytes(layoutImageBytes.data(),
                                              layoutImageBytes.size())}}));

  const fs::path cacheProjectPath = tempDir / "cache_payload.pera";
  const std::vector<std::uint8_t> cacheJson = {'{', '}'};
  const bool cacheSaveOk = saveSession.SaveProject(
      cacheProjectPath.string(),
      [](std::vector<std::uint8_t> &out) {
        out = {'{', '}'};
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out = {'P', 'K'};
        return true;
      },
      ProjectSession::CollectArchiveResourcesFn([&cacheJson] {
        return std::vector<ProjectSession::ArchiveResource>{{
            "resources/layout_view_cache/last_selected_layout_view.json",
            cacheJson}};
      }));
  assert(cacheSaveOk);
  ProjectSession cacheLoadSession;
  assert(cacheLoadSession.LoadProject(
      cacheProjectPath.string(), [](const std::string &) { return true; },
      [](const std::string &) { return true; }));
  assert(cacheLoadSession.GetLoadedArchiveResources().size() == 1);
  assert(cacheLoadSession.GetLoadedArchiveResources().front().bytes ==
         cacheJson);

  const fs::path oversizedCacheProjectPath =
      tempDir / "oversized_cache_payload.pera";
  const bool oversizedCacheSaveOk = saveSession.SaveProject(
      oversizedCacheProjectPath.string(),
      [](std::vector<std::uint8_t> &out) {
        out = {'{', '}'};
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out = {'P', 'K'};
        return true;
      },
      ProjectSession::CollectArchiveResourcesFn([&cacheJson] {
        return std::vector<ProjectSession::ArchiveResource>{
            {"resources/layout_view_cache/rasters/oversized.rgba",
             std::vector<std::uint8_t>(8 * 1024 * 1024 + 1, 7)},
            {"resources/layout_view_cache/last_selected_layout_view.json",
             cacheJson}};
      }));
  assert(oversizedCacheSaveOk);
  auto cacheMetrics = std::make_shared<startup::Metrics>();
  ProjectSession oversizedCacheLoadSession;
  oversizedCacheLoadSession.SetStartupMetrics(cacheMetrics);
  assert(oversizedCacheLoadSession.LoadProject(
      oversizedCacheProjectPath.string(),
      [](const std::string &) { return true; },
      [](const std::string &) { return true; }));
  assert(oversizedCacheLoadSession.GetLoadedArchiveResources().size() == 1);
  assert(oversizedCacheLoadSession.GetLoadedArchiveResources().front().bytes ==
         cacheJson);
  assert(cacheMetrics->cacheEntriesRejected == 1);
  assert(cacheMetrics->cacheEntriesTransferred == 1);

  const fs::path zeroSceneProjectPath = tempDir / "zero_scene.pera";
  ProjectSession zeroSceneSession;
  const bool zeroSceneSaveOk = zeroSceneSession.SaveProject(
      zeroSceneProjectPath.string(),
      [](std::vector<std::uint8_t> &out) {
        const std::string config = "{\"mode\":\"test\"}";
        out.assign(config.begin(), config.end());
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out.clear();
        return true;
      });
  assert(!zeroSceneSaveOk);
  assert(zeroSceneSession.GetLastSaveFailure().kind == ProjectSaveFailureKind::Serialization);
  assert(zeroSceneSession.GetLastSaveFailure().stage == "SerializeScene");
  assert(!fs::exists(zeroSceneProjectPath));

  const fs::path requiredResourceFailurePath =
      tempDir / "required_resource_failure.pera";
  const bool requiredResourceFailure = zeroSceneSession.SaveProject(
      requiredResourceFailurePath.string(),
      [](std::vector<std::uint8_t> &out) {
        out = {'{', '}'};
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out = {'P', 'K'};
        return true;
      },
      ProjectSession::CollectArchiveResourcesFn([] {
        return std::vector<ProjectSession::ArchiveResource>{
            {"../unsafe-manifest.json", {'{', '}'}, true}};
      }));
  assert(!requiredResourceFailure);
  assert(zeroSceneSession.GetLastSaveFailure().kind == ProjectSaveFailureKind::Resource);
  assert(zeroSceneSession.GetLastSaveFailure().entry == "../unsafe-manifest.json");
  assert(!fs::exists(requiredResourceFailurePath));

  // All failures leave an already valid destination unchanged.
  const auto read = [](const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  };
  const auto original = read(projectPath);
  ProjectSession::SaveConfigToBufferFn good = [](auto &bytes) {
    bytes = {'{', '}'};
    return true;
  };
  ProjectSession::SaveConfigToBufferFn bad = [](auto &) { return false; };
  assert(!saveSession.SaveProject(projectPath.string(), bad, good));
  assert(saveSession.GetLastSaveFailure().kind == ProjectSaveFailureKind::Serialization);
  assert(saveSession.GetLastSaveFailure().stage == "SerializeConfig");
  assert(read(projectPath) == original);
  assert(!saveSession.SaveProject(projectPath.string(), good, bad));
  assert(saveSession.GetLastSaveFailure().stage == "SerializeScene");
  assert(read(projectPath) == original);
  // Core returns typed failures; toolkit logging must not open a native dialog.
  UnexpectedWxLog unexpectedWxErrors;
  auto *previousLog = wxLog::SetActiveTarget(&unexpectedWxErrors);
  const auto missingPath = tempDir / "missing" / "fail.pstg";
  const bool missingDestinationSaved =
      saveSession.SaveProject(missingPath.string(), good, good);
  const bool receivedFileOpenError = unexpectedWxErrors.receivedMessage;
  wxLogMessage("Project save logging restoration probe");
  wxLog::SetActiveTarget(previousLog);
  assert(!missingDestinationSaved);
  assert(!receivedFileOpenError);
  assert(unexpectedWxErrors.receivedMessage);
  assert(saveSession.GetLastSaveFailure().kind == ProjectSaveFailureKind::ArchiveIo);
  assert(saveSession.GetLastSaveFailure().stage == "OpenOutput");
  assert(!fs::exists(missingPath));
  assert(!fs::exists(missingPath.string() + ".tmp"));
  assert(read(projectPath) == original);
  ProjectSession::SaveSceneToBufferFn throws = [](auto &) -> bool {
    throw std::runtime_error("test serialization exception");
  };
  assert(!saveSession.SaveProject(projectPath.string(), good, throws));
  assert(saveSession.GetLastSaveFailure().stage == "SerializeScene");
  assert(saveSession.GetLastSaveFailure().detail == "test serialization exception");
  assert(read(projectPath) == original);
  assert(!fs::exists(projectPath.string() + ".tmp"));
  assert(saveSession.SaveProject((tempDir / "success.pstg").string(), good, good));
  assert(saveSession.GetLastSaveFailure().kind == ProjectSaveFailureKind::None);

  const fs::path jsonPath = tempDir / "config_only.json";
  {
    std::ofstream out(jsonPath, std::ios::binary);
    out << "{\"hello\":\"world\"}";
  }

  bool jsonConfigCalled = false;
  bool jsonSceneCalled = false;
  const bool jsonOk = loadSession.LoadProject(
      jsonPath.string(),
      [&](const std::string &path) {
        jsonConfigCalled = true;
        return path == jsonPath.string();
      },
      [&](const std::string &) {
        jsonSceneCalled = true;
        return false;
      });

  assert(jsonOk);
  assert(jsonConfigCalled);
  assert(!jsonSceneCalled);

  fs::remove_all(tempDir, ec);
  return 0;
}
