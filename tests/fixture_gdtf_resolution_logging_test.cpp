#include "fixtures/fixture_gdtf_resolution.h"
#include "projectutils.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <wx/init.h>
#include <wx/log.h>

namespace ProjectUtils {
std::string GetDefaultLibraryPath(const std::string &) { return "/unavailable-library"; }
}

class Log : public wxLog {
public:
  int selected = 0;
  int failed = 0;
  void DoLogRecord(wxLogLevel, const wxString &message, const wxLogRecordInfo &) override {
    selected += message.Contains("selected candidate");
    failed += message.Contains("failed:");
  }
};

int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  Log log;
  wxLog *previous = wxLog::SetActiveTarget(&log);
  wxLog::SetLogLevel(wxLOG_Debug);
  const auto root = std::filesystem::temp_directory_path() / "symbol-resolution-logging";
  std::filesystem::create_directories(root);
  const auto path = root / "fixture.gdtf";
  std::ofstream(path).put('x');
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.gdtfSpec = path.string();
  MvrScene scene;
  gui::fixtures::FixtureGdtfResolution resolution;
  std::string error;
  for (int i = 0; i < 100; ++i)
    assert(gui::fixtures::ResolveFixtureGdtfDeterministic(
        fixture, scene, resolution, error, "symbol-identity"));
  assert(log.selected == 1);
  fixture.gdtfSpec = "missing-one.gdtf";
  assert(!gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution, error));
  fixture.gdtfSpec = "missing-two.gdtf";
  assert(!gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution, error));
  assert(log.failed == 2);
  wxLog::SetActiveTarget(previous);
  std::filesystem::remove_all(root);
  return 0;
}
