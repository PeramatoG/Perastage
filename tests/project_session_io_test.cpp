#include "configservices.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
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
