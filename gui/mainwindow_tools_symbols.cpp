#include "mainwindow.h"

#include "dialogs/generatefixturesymbolsdialog.h"
#include "guiconfigservices.h"
#include "symbols/Symbol2DBuilder.h"
#include "windows/symbolpreviewwindow.h"

#include <map>

#include <wx/utils.h>
#include <wx/msgdlg.h>

void MainWindow::OnGenerateFixtureSymbols(wxCommandEvent &) {
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();

  std::map<std::string, FixtureTypeOption> optionsByType;
  for (const auto &[uuid, fixture] : scene.fixtures) {
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
  for (const auto &[name, option] : optionsByType)
    options.push_back(option);

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
                                         options[selected].gdtfSpec);

  auto *preview = new SymbolPreviewWindow(this, symbols);
  preview->Show();
}
