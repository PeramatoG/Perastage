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
#include "configmanager.h"
#include "guiconfigservices.h"
#include "project_mutation_host.h"
#include "scene_alignment_bounds.h"
#include "selection_movement_settings.h"
#include "diagnostics/DiagnosticLogger.h"
#include <wx/aui/auibar.h>
#include <wx/choice.h>
#include <memory>
#include <optional>
#include <tuple>

namespace {
// Vector glyphs keep edge and center anchors readable at native display DPI.
wxBitmapBundle AlignmentIcon(alignment::Operation operation) {
  using O = alignment::Operation;
  const bool horizontal = operation == O::Left || operation == O::Right || operation == O::CenterHorizontal;
  const int anchor = operation == O::Left || operation == O::Top ? 4 :
                     operation == O::Right || operation == O::Bottom ? 20 : 12;
  const int x0 = anchor == 4 ? 5 : anchor == 20 ? 11 : 8;
  const int x1 = anchor == 4 ? 5 : anchor == 20 ? 7 : 6;
  const std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
      "<g stroke=\"#285082\" stroke-width=\"2\" fill=\"#739bc8\"" +
      std::string(horizontal ? "" : " transform=\"matrix(0 1 1 0 0 0)\"") + ">"
      "<path fill=\"none\" d=\"M" + std::to_string(anchor) + " 2v20\"/>"
      "<rect x=\"" + std::to_string(x0) + "\" y=\"5\" width=\"8\" height=\"5\"/>"
      "<rect x=\"" + std::to_string(x1) + "\" y=\"14\" width=\"12\" height=\"5\"/></g></svg>";
  return wxBitmapBundle::FromSVG(svg.c_str(),wxSize(24,24));
}
scene_grouping::ObjectSelection Selection(const ConfigManager &cfg) {
  return {cfg.GetSelectedFixtures(),cfg.GetSelectedTrusses(),cfg.GetSelectedSupports(),cfg.GetSelectedSceneObjects(),cfg.GetSelectedGroupObjects()};
}
}
// Shared application toolbar, accessible from both scene workspaces.
void MainWindow::CreateAlignmentToolBar() {
  auto *bar = new wxAuiToolBar(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,
                              wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL);
  bar->SetToolBitmapSize(wxSize(24,24));
  auto *reference = new wxChoice(bar,wxID_ANY);
  reference->Append(_("Extents")); reference->Append(_("First selected")); reference->Append(_("Last selected"));
  reference->SetSelection(0); reference->SetToolTip(_("Alignment reference"));
  bar->AddControl(reference);
  auto *plane = new wxChoice(bar,wxID_ANY);
  for (const char *name : {"XY","XZ","YZ"}) plane->Append(wxString::FromUTF8(name));
  plane->SetSelection(0); plane->SetToolTip(_("Alignment work plane: XY (Top), XZ (Front), YZ (Side)"));
  bar->AddControl(plane); bar->AddSeparator();
  struct Availability {
    using Key = std::tuple<size_t,size_t,bool,bool,bool,bool>;
    std::optional<Key> key;
    bool enabled = false;
  };
  const auto availability = std::make_shared<Availability>();
  const wxString labels[] = {_("Align Top"),_("Align Bottom"),_("Align Left"),_("Align Right"),
                              _("Center Horizontally"),_("Center Vertically")};
  for (int index = 0; index < 6; ++index) {
    const int id = wxWindow::NewControlId();
    const auto operation = static_cast<alignment::Operation>(index);
    bar->AddTool(id,labels[index],AlignmentIcon(operation),labels[index]);
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
    Bind(wxEVT_TOOL,[this,reference,plane,operation](wxCommandEvent &) {
      auto &cfg = guiConfigServices->LegacyConfigManager();
      const auto chronology = cfg.GetOrderedSelection();
      auto selection = Selection(cfg);
      GuiProjectMutationHost host(cfg);
      perastage::command::ExecutionContext context{cfg.GetScene(),selection,host};
      const auto result = alignment::Execute(context,chronology,
          static_cast<alignment::Reference>(reference->GetSelection()),operation,
          static_cast<alignment::Plane>(plane->GetSelection()),
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
  auiManager->AddPane(bar,wxAuiPaneInfo().Name("AlignmentToolbar").Caption(_("Alignment"))
      .ToolbarPane().CloseButton(false).Top().Row(1));
}
