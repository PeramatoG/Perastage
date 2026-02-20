#include "mainwindow.h"

#include "dialogs/generatefixturesymbolsdialog.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "guiconfigservices.h"
#include "symbols/Symbol2DBuilder.h"
#include "windows/symbolpreviewwindow.h"

#include <map>

#include <wx/utils.h>
#include <wx/msgdlg.h>

void MainWindow::OnGenerateFixtureSymbols(wxCommandEvent &) {
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();

  std::map<std::string, FixtureTypeOption> optionsByType;
  for (const auto &fixtureEntry : scene.fixtures) {
    const auto &fixture = fixtureEntry.second;
    if (fixture.typeName.empty() || fixture.gdtfSpec.empty())
      continue;
    auto &entry = optionsByType[fixture.typeName];
    if (entry.typeName.empty()) {
      entry.typeName = fixture.typeName;
      entry.gdtfSpec = fixture.gdtfSpec;
    }
    ++entry.instanceCount;
  }

  std::vector<FixtureTypeOption> options;
  options.reserve(optionsByType.size());
  for (const auto &optionEntry : optionsByType)
    options.push_back(optionEntry.second);

  if (options.empty()) {
    wxMessageBox("No fixtures with GDTF type are available in the current project.",
                 "Generate Fixture Symbols", wxOK | wxICON_INFORMATION, this);
    return;
  }

  GenerateFixtureSymbolsDialog dialog(this, options);
  if (dialog.ShowModal() != wxID_OK)
    return;
  const int selected = dialog.GetSelectedIndex();
  if (selected < 0 || selected >= static_cast<int>(options.size()))
    return;

  wxBusyCursor busy;
  symbols::Symbol2DBuilder builder;
  auto symbols = builder.BuildForFixture(options[selected].typeName,
                                         options[selected].gdtfSpec,
                                         scene.basePath);

  std::vector<wxString> generationLog;
  generationLog.push_back(wxString::Format(
      "Symbol generation for type '%s' (spec: %s)",
      wxString::FromUTF8(options[selected].typeName),
      wxString::FromUTF8(options[selected].gdtfSpec)));

  size_t totalFillVertices = 0;
  size_t totalStrokePoints = 0;
  for (const auto &symbol : symbols) {
    size_t fillVertices = 0;
    for (const auto &poly : symbol.fill) {
      fillVertices += poly.outer.size();
      for (const auto &hole : poly.holes)
        fillVertices += hole.size();
    }
    size_t strokePoints = 0;
    for (const auto &stroke : symbol.strokes)
      strokePoints += stroke.points.size();

    totalFillVertices += fillVertices;
    totalStrokePoints += strokePoints;

    generationLog.push_back(wxString::Format(
        "- %s: fill polygons=%zu, fill vertices=%zu, strokes=%zu, stroke points=%zu",
        symbols::ToString(symbol.view), symbol.fill.size(), fillVertices,
        symbol.strokes.size(), strokePoints));
  }
  generationLog.push_back(wxString::Format(
      "Total: fill vertices=%zu, stroke points=%zu", totalFillVertices,
      totalStrokePoints));

  if (ConsolePanel::Instance()) {
    for (const auto &line : generationLog)
      ConsolePanel::Instance()->AppendMessage(line);
  }

  auto *preview = new SymbolPreviewWindow(this, symbols, generationLog);
  preview->Show();
}
