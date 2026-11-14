/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"
#include "main_toolbar_pane_policy.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "project_mutation_host.h"
#include "scene_alignment_bounds.h"
#include "selection_movement_settings.h"
#include "diagnostics/DiagnosticLogger.h"
#include <wx/aui/auibar.h>
#include "projectutils.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "interaction/selection_work_plane.h"
#include <wx/artprov.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <tuple>

namespace {
// Load monochrome vector assets at the standard toolbar size.
wxBitmapBundle AlignmentIcon(const char *name) {
  const auto path = ProjectUtils::ResolveResourcePath(
      std::filesystem::path("icons") / "outline" / (std::string(name) + ".svg"));
  const auto bundle = wxBitmapBundle::FromSVGFile(path.string(),wxSize(16,16));
  return bundle.IsOk() ? bundle : wxArtProvider::GetBitmapBundle(wxART_MISSING_IMAGE,wxART_TOOLBAR,wxSize(16,16));
}
// Supply the same explicit disabled artwork as the Layout toolbars.
void SetAlignmentDisabledIcon(wxAuiToolBar *bar, int id, const char *name) {
  if (auto *item = bar->FindTool(id))
    item->SetDisabledBitmap(AlignmentIcon((std::string(name) + "-disabled").c_str())
                                .GetBitmap(wxSize(16,16)));
}
struct AlignmentControls {
  alignment::Reference reference = alignment::Reference::Extents;
  bool use2D = false;
};
bool ContainsFocus(wxWindow *panel) {
  for (auto *focus = wxWindow::FindFocus(); focus; focus = focus->GetParent())
    if (focus == panel) return true;
  return false;
}
scene_grouping::ObjectSelection Selection(const ConfigManager &cfg) {
  return {cfg.GetSelectedFixtures(),cfg.GetSelectedTrusses(),cfg.GetSelectedSupports(),cfg.GetSelectedSceneObjects(),cfg.GetSelectedGroupObjects()};
}
}
// Shared application toolbar, accessible from both scene workspaces.
void MainWindow::CreateAlignmentToolBar() {
  auto *bar = new wxAuiToolBar(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,
                              wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL);
  bar->SetToolBitmapSize(wxSize(16,16));
  const auto controls = std::make_shared<AlignmentControls>();
  const auto currentPlane = [this,controls]() {
    const bool shown2D = viewport2DPanel && viewport2DPanel->IsShownOnScreen();
    const bool shown3D = viewportPanel && viewportPanel->IsShownOnScreen();
    if (shown2D && (!shown3D || ContainsFocus(viewport2DPanel))) controls->use2D = true;
    else if (!shown2D || (shown3D && ContainsFocus(viewportPanel))) controls->use2D = false;
    if (controls->use2D && shown2D)
      return viewer3d::WorkPlaneForView(viewport2DPanel->GetView());
    return viewportPanel ? viewportPanel->GetSceneWorkPlane() : alignment::Plane::XY;
  };
  const wxString references[] = {_("Extents"),_("First selected"),_("Last selected")};
  const char *referenceIcons[] = {"alignment-extents","alignment-first","alignment-last"};
  for (int index = 0; index < 3; ++index) {
    const int id = wxWindow::NewControlId();
    bar->AddTool(id,references[index],AlignmentIcon(referenceIcons[index]),references[index],wxITEM_RADIO);
    SetAlignmentDisabledIcon(bar,id,referenceIcons[index]);
    bar->ToggleTool(id,index == 0);
    Bind(wxEVT_TOOL,[controls,index](wxCommandEvent &) {
      controls->reference = static_cast<alignment::Reference>(index);
    },id);
    Bind(wxEVT_UPDATE_UI,[this,controls,index](wxUpdateUIEvent &event) {
      event.Enable(!layoutModeActive);
      event.Check(controls->reference == static_cast<alignment::Reference>(index));
    },id);
  }
  bar->AddSeparator();
  const wxString planes[] = {_("XY work plane (Top)"),_("XZ work plane (Front)"),_("YZ work plane (Side)")};
  const char *planeIcons[] = {"work-plane-xy","work-plane-xz","work-plane-yz"};
  for (int index = 0; index < 3; ++index) {
    const int id = wxWindow::NewControlId();
    bar->AddTool(id,planes[index],AlignmentIcon(planeIcons[index]),planes[index],wxITEM_RADIO);
    SetAlignmentDisabledIcon(bar,id,planeIcons[index]);
    bar->ToggleTool(id,index == 0);
    Bind(wxEVT_TOOL,[this,index](wxCommandEvent &) {
      if (viewportPanel) viewportPanel->SetSceneWorkPlane(static_cast<alignment::Plane>(index));
    },id);
    Bind(wxEVT_UPDATE_UI,[this,controls,currentPlane,index](wxUpdateUIEvent &event) {
      const auto plane = currentPlane();
      event.Enable(!layoutModeActive && !controls->use2D && viewportPanel);
      event.Check(plane == static_cast<alignment::Plane>(index));
    },id);
  }
  bar->AddSeparator();
  struct Availability {
    using Key = std::tuple<size_t,size_t,bool,bool,bool,bool>;
    std::optional<Key> key;
    bool enabled = false;
  };
  const auto availability = std::make_shared<Availability>();
  const wxString labels[] = {_("Align Top"),_("Align Bottom"),_("Align Left"),_("Align Right"),
                              _("Center Horizontally"),_("Center Vertically")};
  const char *operationIcons[] = {"alignment-top","alignment-bottom","alignment-left","alignment-right",
                                  "alignment-center-horizontal","alignment-center-vertical"};
  for (int index = 0; index < 6; ++index) {
    const int id = wxWindow::NewControlId();
    const auto operation = static_cast<alignment::Operation>(index);
    bar->AddTool(id,labels[index],AlignmentIcon(operationIcons[index]),labels[index]);
    SetAlignmentDisabledIcon(bar,id,operationIcons[index]);
    Bind(wxEVT_UPDATE_UI,[this,availability](wxUpdateUIEvent &event) {
      auto &cfg = guiConfigServices->LegacyConfigManager();
      const auto policy = selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
      const Availability::Key key{cfg.GetRenderRevision(),cfg.GetSelectionRevision(),
          policy.promoteFixturesToGroup,policy.promoteTrussesToGroup,
          policy.promoteSupportsToGroup,policy.promoteSceneObjectsToGroup};
      if (availability->key != key) {
        const auto targets = alignment::ResolveTargets(cfg.GetScene(),cfg.GetOrderedSelection(),policy);
        availability->enabled = targets.diagnostic.empty() && targets.items.size() >= 2;
        availability->key = key;
      }
      event.Enable(!layoutModeActive && availability->enabled);
    },id);
    Bind(wxEVT_TOOL,[this,controls,currentPlane,operation](wxCommandEvent &) {
      auto &cfg = guiConfigServices->LegacyConfigManager();
      const auto chronology = cfg.GetOrderedSelection();
      auto selection = Selection(cfg);
      GuiProjectMutationHost host(cfg);
      perastage::command::ExecutionContext context{cfg.GetScene(),selection,host};
      const auto result = alignment::Execute(context,chronology,
          controls->reference,operation,currentPlane(),
          selection_movement_settings::LoadInteractiveTransformPolicy(cfg),alignment::SceneBounds);
      for (const auto &diagnostic : result.diagnostics) {
        using S = perastage::command::DiagnosticSeverity;
        if (diagnostic.severity == S::Error) diagnostics::DiagnosticLogger::Error(diagnostic.message);
        else if (diagnostic.severity == S::Warning) diagnostics::DiagnosticLogger::Warning(diagnostic.message);
        else diagnostics::DiagnosticLogger::Info(diagnostic.message);
      }
      if (!result.Success()) { SetStatusText(_("Alignment failed. See diagnostics for details.")); return; }
      if (result.mutation.sceneChanged) RefreshAfterToolSceneUpdate(selection);
      SetStatusText(result.mutation.sceneChanged ? _("Selection aligned.") : _("Selection is already aligned."));
    },id);
  }
  bar->Realize();
  int position = 0;
  for (const auto name : gui::kAlwaysVisibleToolbarPanes) {
    if (name == "AlignmentToolbar") continue;
    const auto &pane = auiManager->GetPane(std::string(name));
    if (pane.IsOk() && pane.window) position += pane.window->GetBestSize().x;
  }
  const bool fits = position + bar->GetBestSize().x <= GetClientSize().x;
  auiManager->AddPane(bar,wxAuiPaneInfo().Name("AlignmentToolbar").Caption(_("Alignment"))
      .ToolbarPane().CloseButton(false).Top().Row(fits ? 0 : 1).Position(fits ? position : 0));
}
