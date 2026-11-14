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
#include "projectutils.h"
#include <wx/aui/auibar.h>
#include <wx/choice.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

namespace {
// Conventional edge/center glyphs drawn once in logical toolbar pixels.
wxBitmapBundle AlignmentIcon(alignment::Operation operation) {
  wxBitmap bitmap(24,24,32);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(*wxWHITE_BRUSH); dc.Clear();
  dc.SetPen(wxPen(wxColour(40,80,130),2));
  dc.SetBrush(wxBrush(wxColour(115,155,200)));
  using O = alignment::Operation;
  const bool horizontal = operation == O::Left || operation == O::Right || operation == O::CenterHorizontal;
  const int anchor = operation == O::Left || operation == O::Top ? 4 :
                     operation == O::Right || operation == O::Bottom ? 20 : 12;
  if (horizontal) {
    dc.DrawLine(anchor,2,anchor,22);
    dc.DrawRectangle(operation == O::Left ? 5 : operation == O::Right ? 11 : 8,5,8,5);
    dc.DrawRectangle(operation == O::Left ? 5 : operation == O::Right ? 7 : 6,14,12,5);
  } else {
    dc.DrawLine(2,anchor,22,anchor);
    dc.DrawRectangle(5,operation == O::Top ? 5 : operation == O::Bottom ? 11 : 8,5,8);
    dc.DrawRectangle(14,operation == O::Top ? 5 : operation == O::Bottom ? 7 : 6,5,12);
  }
  dc.SelectObject(wxNullBitmap);
  return wxBitmapBundle::FromBitmap(bitmap);
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
  plane->Append("XY"); plane->Append("XZ"); plane->Append("YZ");
  plane->SetSelection(0); plane->SetToolTip(_("Alignment work plane: XY (Top), XZ (Front), YZ (Side)"));
  bar->AddControl(plane); bar->AddSeparator();
  const wxString labels[] = {_("Align Top"),_("Align Bottom"),_("Align Left"),_("Align Right"),
                              _("Center Horizontally"),_("Center Vertically")};
  for (int index = 0; index < 6; ++index) {
    const int id = wxWindow::NewControlId();
    const auto operation = static_cast<alignment::Operation>(index);
    bar->AddTool(id,labels[index],AlignmentIcon(operation),labels[index]);
    Bind(wxEVT_UPDATE_UI,[this](wxUpdateUIEvent &event) {
      auto &cfg = guiConfigServices->LegacyConfigManager();
      const auto targets = alignment::ResolveTargets(cfg.GetScene(),cfg.GetOrderedSelection(),
          selection_movement_settings::LoadInteractiveTransformPolicy(cfg));
      event.Enable(!layoutModeActive && targets.diagnostic.empty() && targets.items.size() >= 2);
    },id);
    Bind(wxEVT_TOOL,[this,reference,plane,operation](wxCommandEvent &) {
      auto &cfg = guiConfigServices->LegacyConfigManager();
      auto selection = Selection(cfg);
      const auto chronology = cfg.GetOrderedSelection();
      GuiProjectMutationHost host(cfg);
      perastage::command::ExecutionContext context{cfg.GetScene(),selection,host};
      const auto result = alignment::Execute(context,chronology,
          static_cast<alignment::Reference>(reference->GetSelection()),operation,
          static_cast<alignment::Plane>(plane->GetSelection()),
          selection_movement_settings::LoadInteractiveTransformPolicy(cfg),alignment::SceneBounds);
      for (const auto &diagnostic : result.diagnostics)
        diagnostics::DiagnosticLogger::Warning(diagnostic.message);
      if (!result.Success()) { SetStatusText(_("Alignment failed. See diagnostics for details.")); return; }
      if (result.mutation.sceneChanged) RefreshAfterToolSceneUpdate(selection);
      SetStatusText(result.mutation.sceneChanged ? _("Selection aligned.") : _("Selection is already aligned."));
    },id);
  }
  bar->Realize();
  auiManager->AddPane(bar,wxAuiPaneInfo().Name("AlignmentToolbar").Caption(_("Alignment"))
      .ToolbarPane().CloseButton(false).Top().Row(1));
}
