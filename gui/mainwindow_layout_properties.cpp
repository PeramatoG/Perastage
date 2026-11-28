// This file is part of Perastage. License: GPL-3.0-or-later.
#include "mainwindow.h"

#include "LayoutManager.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "layout_fixture_labels.h"
#include "layout_properties_provider.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"

namespace {
constexpr auto kPane = "LayoutProperties";
constexpr auto kVisibilityKey = "ui_layout_properties_visible";
constexpr auto kPerspectiveKey = "ui_layout_mode_perspective";

auto &Manager() { return layouts::LayoutManager::Get(); }

void StorePreference(IGuiConfigServices &services, const char *key, const std::string &value) {
  auto &config = services.LegacyConfigManager();
  const auto before = config.CaptureDirtyState();
  services.Preferences().SetValue(key, value);
  config.RestoreDirtyState(before);
}
} // namespace

void MainWindow::CreateLayoutProperties() {
  struct State {
    gui::layoutproperties::Context context;
    size_t generation = 0;
  };
  const auto state = std::make_shared<State>();
  const auto getContext = [this] {
    return gui::layoutproperties::Context{
        layoutModeActive ? activeLayoutName : std::string{},
        layoutViewerPanel->SelectedElements(), Manager().Revision()};
  };
  const auto commit = [this](int id, const gui::properties::Value &value) {
    using namespace gui::layoutproperties;
    const Context context{activeLayoutName, layoutViewerPanel->SelectedElements(), Manager().Revision()};
    const auto property = static_cast<Property>(id);
    if (property == Advanced) return layoutViewerPanel->EditSelectedProperties();
    if (property == Labels) {
      if (context.selection.size() != 1 || context.selection.front().kind != layouts::LayoutElementKind::View2D)
        return false;
      EditLayoutFixtureLabels(*layoutViewerPanel, context.page, context.selection.front().id);
      return true;
    }
    if (property == BringFront || property == SendBack) {
      if (context.selection.size() != 1) return false;
      layoutViewerPanel->StackSelectedElement(property == BringFront);
      return true;
    }
    if (!Edit(Manager(), context, property, value)) return false;
    if (context.revision == Manager().Revision()) return true;

    if (property == Name) {
      auto name = std::get<wxString>(value);
      name.Trim().Trim(false);
      activeLayoutName = name.utf8_string();
      layoutPanel->SetCurrentLayout(activeLayoutName);
      layoutPanel->ReloadLayouts();
      return true;
    }
    for (const auto &page : Manager().GetLayouts().Items()) {
      if (page.name != context.page) continue;
      layoutViewerPanel->ApplyInspectorSnapshot(page, property == X || property == Y);
      break;
    }
    return true;
  };
  layoutPropertiesPanel = new gui::properties::Panel(this,
      [state, getContext] {
        const auto context = getContext();
        if (state->context != context) {
          state->context = context;
          ++state->generation;
        }
        const auto &collection = Manager().GetLayouts();
        auto sheet = gui::layoutproperties::Build(gui::layoutproperties::FindPage(collection, context.page),
                                                  collection.labelProfiles, context);
        sheet.generation = state->generation;
        return sheet;
      },
      [this, state, getContext, commit](int id, const gui::properties::Value &value) {
        if (state->context != getContext() || layout2DViewEditing) return false;
        return commit(id, value);
      });
  auiManager->AddPane(layoutPropertiesPanel, wxAuiPaneInfo()
      .Name(kPane).Caption(_("Properties")).Right().Layer(0).Row(0).Position(0)
      .BestSize(FromDIP(wxSize(320, 500))).MinSize(FromDIP(wxSize(215, 160)))
      .CloseButton(true).Floatable(true).Resizable(true).Hide());
  layoutViewerPanel->SetLayoutSelectionHandler([this] { RefreshLayoutProperties(); });
  layoutPropertiesSubscription = guiConfigServices->LegacyConfigManager().SubscribeChanges(
      [this](ConfigManager::Change change) {
        if (change == ConfigManager::Change::LayoutDocument) RefreshLayoutProperties();
      });
}

void MainWindow::RefreshLayoutProperties() {
  if (layoutModeActive && layoutPropertiesPanel && auiManager->GetPane(kPane).IsShown())
    layoutPropertiesPanel->RequestRefresh();
}

void MainWindow::OnToggleLayoutProperties(wxCommandEvent &) {
  if (!layoutModeActive || !auiManager) return;
  auto &pane = auiManager->GetPane(kPane);
  pane.Show(!pane.IsShown());
  SaveLayoutPropertiesVisibility(pane.IsShown());
  auiManager->Update();
  UpdateViewMenuChecks();
  RefreshLayoutProperties();
}

void MainWindow::SaveLayoutPropertiesVisibility(bool shown) {
  StorePreference(*guiConfigServices, kVisibilityKey, shown ? "1" : "0");
}

void MainWindow::RestoreLayoutPropertiesVisibility(bool layoutMode) {
  const bool shown = guiConfigServices->Preferences().GetValue(kVisibilityKey).value_or("1") != "0";
  auiManager->GetPane(kPane).Show(layoutMode && shown);
}

std::optional<std::string> MainWindow::ReadLayoutWorkspacePerspective() const {
  return guiConfigServices->Preferences().GetValue(kPerspectiveKey);
}

void MainWindow::SaveLayoutWorkspacePerspective() {
  if (auiManager && layoutModeActive)
    StorePreference(*guiConfigServices, kPerspectiveKey, auiManager->SavePerspective().utf8_string());
}
