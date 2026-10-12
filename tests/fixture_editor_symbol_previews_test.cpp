#include "gdtf/fixture_symbol_comparison_panel.h"
#include "gdtf/fixture_thumbnail_preview.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_test_fixture_builder.h"
#include "symbols/project_fixture_symbol_runtime.h"
#include "mvrscene.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/mstream.h>
#include <wx/tooltip.h>
#include <tinyxml2.h>

namespace {
void Collect(wxWindow *parent, std::vector<wxWindow *> &windows) {
  for (auto *child : parent->GetChildren()) {
    windows.push_back(child);
    Collect(child, windows);
  }
}
std::vector<wxString> Tips(wxWindow *parent) {
  std::vector<wxWindow *> windows;
  Collect(parent, windows);
  std::vector<wxString> result;
  for (auto *window : windows)
    if (window->GetToolTip())
      result.push_back(window->GetToolTip()->GetTip());
  return result;
}
} // namespace

void CheckFixtureEditorSymbolsAndThumbnails() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() / "perastage-fixture-editor-previews";
  fs::remove_all(root);
  fs::create_directories(root);
  const std::string svg =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">"
      "<path d=\"M10 10 C90 10 90 90 10 90 Z\" fill=\"#ff0000\"/></svg>";
  wxInitAllImageHandlers();
  wxImage png(8, 8);
  png.SetRGB(wxRect(0, 0, 8, 8), 0, 255, 0);
  wxMemoryOutputStream output;
  assert(png.SaveFile(output, wxBITMAP_TYPE_PNG));
  std::string pngBytes(output.GetSize(), '\0');
  output.CopyTo(pngBytes.data(), pngBytes.size());
  const auto seed = root / "seed.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("base")
      .WithFixtureTypeExtensionAttribute("Thumbnail", "thumb")
      .WithArchiveEntry("models/svg/base.svg", svg)
      .WithArchiveEntry("models/svg/yoke.svg", svg)
      .WithArchiveEntry("thumb.svg", svg)
      .WithArchiveEntry("thumb.png", pngBytes)
      .WithArchiveEntry("thumbnails/thumb.png", pngBytes)
      .WriteArchive(seed);
  const auto fixture = root / "fixture.gdtf";
  assert(GdtfCanonicalizer::RewriteArchiveDescription(seed, fixture,
      [](tinyxml2::XMLDocument &document) {
        auto *models = document.FirstChildElement("GDTF")->FirstChildElement("FixtureType")
            ->FirstChildElement("Models");
        auto *second = document.NewElement("Model");
        second->SetAttribute("Name", "Yoke");
        second->SetAttribute("File", "yoke");
        second->SetAttribute("SVGOffsetX", 3.5);
        models->InsertEndChild(second);
        return true;
      }).success);

  wxFrame frame(nullptr, wxID_ANY, "Fixture preview test");
  auto *panel = new FixtureSymbolComparisonPanel(&frame);
  symbols::ProjectFixtureSymbolStore store;
  MvrScene scene;
  Fixture selected;
  selected.uuid = "12345678-1234-4234-9234-123456789abc";
  scene.fixtures.emplace(selected.uuid, selected);
  symbols::ProjectFixtureSymbolBundle bundle;
  bundle.kind = symbols::ProjectFixtureSymbolKind::UserOverride;
  bundle.generatorVersion = "test";
  for (auto &view : bundle.views)
    view.svg = "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  std::string error;
  assert(store.ApplyToFixtures(scene, {selected.uuid}, bundle, error));
  symbols::ScopedProjectFixtureSymbolReadContext context(
      std::make_shared<const symbols::ProjectFixtureSymbolStore>(store));
  panel->SetArchivePath(fixture.string(),
      symbols::BuildProjectFixtureSymbolSource(scene.fixtures.at(selected.uuid)));
  std::vector<wxWindow *> windows;
  Collect(panel, windows);
  wxChoice *selector = nullptr;
  for (auto *window : windows)
    if (auto *choice = dynamic_cast<wxChoice *>(window))
      selector = choice;
  assert(selector && selector->GetCount() == 2 && selector->GetSelection() == 0);
  const auto originalTips = Tips(panel);
  assert(std::any_of(originalTips.begin(), originalTips.end(), [](const auto &tip) {
    return tip.Contains("models/svg/base.svg");
  }));
  selector->SetSelection(1);
  wxCommandEvent change(wxEVT_CHOICE, selector->GetId());
  change.SetEventObject(selector);
  selector->GetEventHandler()->ProcessEvent(change);
  const auto selectedTips = Tips(panel);
  assert(std::none_of(selectedTips.begin(), selectedTips.end(), [](const auto &tip) {
    return tip.Contains("models/svg/base.svg");
  }));
  assert(std::any_of(selectedTips.begin(), selectedTips.end(), [](const auto &tip) {
    return tip.Contains("models/svg/yoke.svg") && tip.Contains("3.5");
  }));
  for (const auto &tip : selectedTips)
    assert(!tip.Contains("svg_bottom"));
  std::vector<wxString> projectBefore, projectAfter;
  for (const auto &tip : originalTips)
    if (tip.Contains("User override")) projectBefore.push_back(tip);
  for (const auto &tip : selectedTips)
    if (tip.Contains("User override")) projectAfter.push_back(tip);
  assert(projectBefore.size() == 4 && projectBefore == projectAfter);

  const auto thumbnail = LoadGdtfThumbnailPreview(fixture.string(), wxSize(80, 80), *wxWHITE);
  assert(thumbnail.decoded && thumbnail.diagnostic.starts_with("thumb.svg (canonical)"));
  // SVG-only and PNG-only canonical thumbnails both use the production backend.
  for (const auto svgOnly : {true, false}) {
    const auto path = root / (svgOnly ? "svg.gdtf" : "png.gdtf");
    tests::gdtf::BuildMinimalValidFixture().WithFixtureTypeExtensionAttribute("Thumbnail", "thumb")
        .WithArchiveEntry(svgOnly ? "thumb.svg" : "thumb.png", svgOnly ? svg : pngBytes)
        .WithArchiveEntry("thumbnails/thumb.png", pngBytes).WriteArchive(path);
    const auto result = LoadGdtfThumbnailPreview(path.string(), wxSize(80, 80), *wxWHITE);
    assert(result.decoded);
    assert(result.diagnostic.find("(canonical)") != std::string::npos);
    assert(result.diagnostic.find("thumbnails/") == std::string::npos);
  }
  const auto unsupported = root / "unsupported.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithFixtureTypeExtensionAttribute("Thumbnail", "thumb")
      .WithArchiveEntry("thumb.svg", "<svg")
      .WithArchiveEntry("thumbnails/thumb.png", pngBytes).WriteArchive(unsupported);
  const auto unsupportedPreview = LoadGdtfThumbnailPreview(
      unsupported.string(), wxSize(80, 80), *wxWHITE);
  assert(!unsupportedPreview.decoded);
  assert(unsupportedPreview.diagnostic.find("(canonical)") != std::string::npos);
  assert(unsupportedPreview.diagnostic.find("thumbnails/") == std::string::npos);
  fs::remove_all(root);
}
