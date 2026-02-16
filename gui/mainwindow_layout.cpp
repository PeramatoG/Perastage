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

#include <algorithm>
#include <cmath>

#include <wx/notebook.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "layout2dviewdialog.h"
#include "layoutimageutils.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"
#include "layoutviewpresets.h"
#include "layerpanel.h"
#include "riggingpanel.h"
#include "sceneobjecttablepanel.h"
#include "summarypanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "LayoutManager.h"

namespace {
layouts::Layout2DViewFrame BuildDefaultLayout2DFrame(
    const layouts::LayoutDefinition &layout) {
  constexpr double kFrameScale = 0.6;
  constexpr int kMinFrameSize = 120;

  const double pageWidth = layout.pageSetup.PageWidthPt();
  const double pageHeight = layout.pageSetup.PageHeightPt();

  int width = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageWidth * kFrameScale)));
  int height = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageHeight * kFrameScale)));

  width = std::min(width, static_cast<int>(std::lround(pageWidth)));
  height = std::min(height, static_cast<int>(std::lround(pageHeight)));

  int x =
      std::max(0, static_cast<int>(std::lround((pageWidth - width) / 2.0)));
  int y =
      std::max(0, static_cast<int>(std::lround((pageHeight - height) / 2.0)));

  return {x, y, width, height};
}

layouts::Layout2DViewFrame BuildDefaultLayoutLegendFrame(
    const layouts::LayoutDefinition &layout) {
  constexpr double kWidthScale = 0.35;
  constexpr double kHeightScale = 0.4;
  constexpr int kMinFrameSize = 120;
  constexpr int kMargin = 20;

  const double pageWidth = layout.pageSetup.PageWidthPt();
  const double pageHeight = layout.pageSetup.PageHeightPt();

  int width = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageWidth * kWidthScale)));
  int height = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageHeight * kHeightScale)));

  width = std::min(width, static_cast<int>(std::lround(pageWidth)));
  height = std::min(height, static_cast<int>(std::lround(pageHeight)));

  int x = std::max(
      0, static_cast<int>(std::lround(pageWidth - width - kMargin)));
  int y = std::max(0, kMargin);

  return {x, y, width, height};
}

layouts::Layout2DViewFrame BuildDefaultLayoutEventTableFrame(
    const layouts::LayoutDefinition &layout) {
  constexpr double kWidthScale = 0.45;
  constexpr double kHeightScale = 0.3;
  constexpr int kMinFrameSize = 140;
  constexpr int kMargin = 20;

  const double pageWidth = layout.pageSetup.PageWidthPt();
  const double pageHeight = layout.pageSetup.PageHeightPt();

  int width = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageWidth * kWidthScale)));
  int height = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageHeight * kHeightScale)));

  width = std::min(width, static_cast<int>(std::lround(pageWidth)));
  height = std::min(height, static_cast<int>(std::lround(pageHeight)));

  int x = std::max(0, kMargin);
  int y = std::max(
      0, static_cast<int>(std::lround(pageHeight - height - kMargin)));

  return {x, y, width, height};
}

layouts::Layout2DViewFrame BuildDefaultLayoutTextFrame(
    const layouts::LayoutDefinition &layout) {
  constexpr double kWidthScale = 0.35;
  constexpr double kHeightScale = 0.14;
  constexpr int kMinFrameSize = 80;
  constexpr int kMargin = 20;

  const double pageWidth = layout.pageSetup.PageWidthPt();
  const double pageHeight = layout.pageSetup.PageHeightPt();

  int width = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageWidth * kWidthScale)));
  int height = std::max(
      kMinFrameSize,
      static_cast<int>(std::lround(pageHeight * kHeightScale)));

  width = std::min(width, static_cast<int>(std::lround(pageWidth)));
  height = std::min(height, static_cast<int>(std::lround(pageHeight)));

  int x =
      std::max(0, static_cast<int>(std::lround((pageWidth - width) / 2.0)));
  int y = std::max(0, kMargin);

  return {x, y, width, height};
}

layouts::Layout2DViewFrame BuildDefaultLayoutImageFrame(
    const layouts::LayoutDefinition &layout, double aspectRatio) {
  constexpr double kMaxScale = 0.4;
  constexpr int kMinFrameSize = 120;

  const double pageWidth = layout.pageSetup.PageWidthPt();
  const double pageHeight = layout.pageSetup.PageHeightPt();
  const double ratio = aspectRatio > 0.0 ? aspectRatio : 1.0;

  double width = pageWidth * kMaxScale;
  double height = width / ratio;
  if (height > pageHeight * kMaxScale) {
    height = pageHeight * kMaxScale;
    width = height * ratio;
  }

  if (width < kMinFrameSize || height < kMinFrameSize) {
    const double scale =
        std::max(kMinFrameSize / width, kMinFrameSize / height);
    width *= scale;
    height *= scale;
  }

  const double maxScale = std::min(pageWidth / width, pageHeight / height);
  if (maxScale < 1.0) {
    width *= maxScale;
    height *= maxScale;
  }

  int finalWidth = static_cast<int>(std::lround(width));
  int finalHeight = static_cast<int>(std::lround(height));

  int x = std::max(
      0, static_cast<int>(std::lround((pageWidth - finalWidth) / 2.0)));
  int y = std::max(
      0, static_cast<int>(std::lround((pageHeight - finalHeight) / 2.0)));

  return {x, y, finalWidth, finalHeight};
}

const layouts::Layout2DViewDefinition *FindLayout2DViewById(
    const layouts::LayoutDefinition *layout, int viewId) {
  if (!layout || viewId <= 0)
    return nullptr;
  for (const auto &view : layout->view2dViews) {
    if (view.id == viewId)
      return &view;
  }
  return nullptr;
}
}

void MainWindow::SetupLayout() {
  CreateMenuBar();

  // Initialize AUI manager for dynamic pane layout
  auiManager = new wxAuiManager(this);
  Bind(wxEVT_AUI_PANE_CLOSE, &MainWindow::OnPaneClose, this);

  CreateToolBars();

  // Create notebook with data panels
  notebook = new wxNotebook(this, wxID_ANY);
  notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,
                 &MainWindow::OnNotebookPageChanged, this);

  fixturePanel = new FixtureTablePanel(notebook, guiConfigServices);
  FixtureTablePanel::SetInstance(fixturePanel);
  notebook->AddPage(fixturePanel, "Fixtures");

  trussPanel = new TrussTablePanel(notebook, guiConfigServices);
  TrussTablePanel::SetInstance(trussPanel);
  notebook->AddPage(trussPanel, "Trusses");

  hoistPanel = new HoistTablePanel(notebook, guiConfigServices);
  HoistTablePanel::SetInstance(hoistPanel);
  notebook->AddPage(hoistPanel, "Hoists");

  sceneObjPanel = new SceneObjectTablePanel(notebook, guiConfigServices);
  SceneObjectTablePanel::SetInstance(sceneObjPanel);
  notebook->AddPage(sceneObjPanel, "Objects");

  // Add notebook on the left so the viewport can occupy
  // the remaining (and larger) central area
  int halfWidth = GetClientSize().GetWidth() / 2;

  auiManager->AddPane(notebook, wxAuiPaneInfo()
                                    .Name("DataNotebook")
                                    .Caption("Data Views")
                                    .Left()
                                    .BestSize(halfWidth, 600)
                                    .MinSize(wxSize(200, 300))
                                    .PaneBorder(false)
                                    .CaptionVisible(true)
                                    .CloseButton(true)
                                    .MaximizeButton(true));

  // Bottom console panel for messages
  consolePanel = new ConsolePanel(this);
  ConsolePanel::SetInstance(consolePanel);
  auiManager->AddPane(consolePanel, wxAuiPaneInfo()
                                        .Name("Console")
                                        .Caption("Console")
                                        .Bottom()
                                        .BestSize(-1, 150)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  layerPanel = new LayerPanel(this);
  LayerPanel::SetInstance(layerPanel);
  auiManager->AddPane(layerPanel, wxAuiPaneInfo()
                                      .Name("LayerPanel")
                                      .Caption("Layers")
                                      .Right()
                                      .BestSize(200, 300)
                                      .CloseButton(true)
                                      .MaximizeButton(true)
                                      .PaneBorder(true));

  layoutPanel = new LayoutPanel(this);
  LayoutPanel::SetInstance(layoutPanel);
  auiManager->AddPane(layoutPanel, wxAuiPaneInfo()
                                       .Name("LayoutPanel")
                                       .Caption("Layouts")
                                       .Left()
                                       .Row(0)
                                       .Position(1)
                                       .BestSize(240, 260)
                                       .MinSize(wxSize(200, 200))
                                       .CloseButton(true)
                                       .MaximizeButton(true)
                                       .PaneBorder(true)
                                       .Hide());

  layoutViewerPanel = new LayoutViewerPanel(this);
  auiManager->AddPane(layoutViewerPanel, wxAuiPaneInfo()
                                            .Name("LayoutViewer")
                                            .Caption("Layout Viewer")
                                            .Center()
                                            .Dockable(true)
                                            .CaptionVisible(true)
                                            .PaneBorder(false)
                                            .BestSize(halfWidth, 600)
                                            .MinSize(wxSize(200, 300))
                                            .CloseButton(true)
                                            .MaximizeButton(true)
                                            .Hide());

  summaryPanel = new SummaryPanel(this);
  SummaryPanel::SetInstance(summaryPanel);
  auiManager->AddPane(summaryPanel, wxAuiPaneInfo()
                                        .Name("SummaryPanel")
                                        .Caption("Summary")
                                        .Right()
                                        .Row(1)
                                        .Position(0)
                                        .BestSize(200, 150)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  riggingPanel = new RiggingPanel(this);
  RiggingPanel::SetInstance(riggingPanel);
  auiManager->AddPane(riggingPanel, wxAuiPaneInfo()
                                        .Name("RiggingPanel")
                                        .Caption("Rigging")
                                        .Right()
                                        .Row(1)
                                        .Position(1)
                                        .BestSize(250, 200)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  // Apply all changes to layout
  auiManager->Update();

  if (summaryPanel)
    summaryPanel->ShowFixtureSummary();
  if (riggingPanel)
    riggingPanel->RefreshData();

  // Keyboard shortcuts to switch notebook pages
  wxAcceleratorEntry entries[4];
  entries[0].Set(wxACCEL_NORMAL, (int)'1', ID_Select_Fixtures);
  entries[1].Set(wxACCEL_NORMAL, (int)'2', ID_Select_Trusses);
  entries[2].Set(wxACCEL_NORMAL, (int)'3', ID_Select_Supports);
  entries[3].Set(wxACCEL_NORMAL, (int)'4', ID_Select_Objects);
  m_accel = wxAcceleratorTable(4, entries);
  SetAcceleratorTable(m_accel);

  // Ensure the View menu reflects the actual pane visibility
  UpdateViewMenuChecks();
}

void MainWindow::ApplyLayoutPreset(const LayoutViewPreset &preset,
                                   const std::optional<std::string> &perspective,
                                   bool layoutMode,
                                   bool persistPerspective) {
  if (!auiManager)
    return;

  const bool wasLayoutMode = layoutModeActive;
  if (layoutMode && !wasLayoutMode) {
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    if (!standalone2DState)
      standalone2DState = viewer2d::CaptureState(viewport2DPanel, cfg);
  } else if (!layoutMode && wasLayoutMode) {
    if (standalone2DState) {
      ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      if (viewport2DPanel) {
        const auto viewState = viewport2DPanel->GetViewState();
        standalone2DState->camera.offsetPixelsX = viewState.offsetPixelsX;
        standalone2DState->camera.offsetPixelsY = viewState.offsetPixelsY;
        standalone2DState->camera.zoom = viewState.zoom;
        standalone2DState->camera.viewportWidth = viewState.viewportWidth;
        standalone2DState->camera.viewportHeight = viewState.viewportHeight;
        standalone2DState->camera.view = static_cast<int>(viewState.view);
      } else {
        standalone2DState->camera = viewer2d::CaptureState(nullptr, cfg).camera;
      }
      if (viewport2DPanel) {
        viewer2d::ApplyState(viewport2DPanel, viewport2DRenderPanel, cfg,
                             *standalone2DState);
      } else {
        viewer2d::ApplyState(nullptr, nullptr, cfg, *standalone2DState);
      }
      SyncLayerVisibilityPanels();
      standalone2DState.reset();
    }
  }

  if (perspective && !perspective->empty())
    auiManager->LoadPerspective(*perspective, true);
  else
    auiManager->Update();

  auto applyPaneState = [this](const std::vector<std::string> &panes,
                               bool show) {
    for (const auto &name : panes) {
      auto &pane = auiManager->GetPane(name);
      if (pane.IsOk())
        pane.Show(show);
    }
  };

  applyPaneState(preset.showPanes, true);
  applyPaneState(preset.hidePanes, false);

  auiManager->Update();

  layoutModeActive = layoutMode;

  if (persistPerspective) {
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.SetValue("layout_view_mode", preset.name);
    if (auiManager) {
      cfg.SetValue("layout_perspective",
                   auiManager->SavePerspective().ToStdString());
    }
  }

  UpdateViewMenuChecks();
}

void MainWindow::ApplySavedLayout() {
  // Flow overview: choose which perspective to apply (layout mode/2D/3D) from
  // saved config, ensuring viewports exist before restoring; then re-apply
  // minimum sizes so the saved perspective cannot degrade the UI.
  if (!auiManager)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const std::string viewMode = cfg.GetValue("layout_view_mode").value_or("");
  std::optional<std::string> perspective = cfg.GetValue("layout_perspective");

  if (perspective) {
    // Ensure viewports exist before loading the saved perspective
    if (perspective->find("3DViewport") != std::string::npos)
      Ensure3DViewport();
    if (perspective->find("2DViewport") != std::string::npos ||
        perspective->find("2DRenderOptions") != std::string::npos)
      Ensure2DViewport();
  }

  const LayoutViewPreset *preset = LayoutViewPresetRegistry::GetPreset(viewMode);
  if (!preset && perspective &&
      (perspective->find("2DViewport") != std::string::npos ||
       perspective->find("2DRenderOptions") != std::string::npos)) {
    // Backward compatibility for configs without layout_view_mode.
    preset = LayoutViewPresetRegistry::GetPreset("2d_layout_view");
  }
  if (!preset)
    preset = LayoutViewPresetRegistry::GetPreset("3d_layout_view");
  const bool savedLayoutMode = preset && preset->name == "layout_mode_view";
  if (preset && perspective)
    ApplyLayoutPreset(*preset, perspective, savedLayoutMode, false);
  else if (preset)
    ApplyLayoutPreset(*preset, std::nullopt, savedLayoutMode, false);

  // Re-apply hard-coded minimum sizes so they are not overridden by the saved perspective
  auto &dataPane = auiManager->GetPane("DataNotebook");
  if (dataPane.IsOk())
    dataPane.MinSize(wxSize(250, 300));

  auto &view3dPane = auiManager->GetPane("3DViewport");
  if (view3dPane.IsOk())
    view3dPane.MinSize(wxSize(250, 600));

  auto &view2dPane = auiManager->GetPane("2DViewport");
  if (view2dPane.IsOk())
    view2dPane.MinSize(wxSize(250, 600));

  auiManager->Update();
  SendSizeEvent();
  UpdateViewMenuChecks();
}

void MainWindow::ApplyLayoutModePerspective() {
  if (!auiManager)
    return;

  const auto *preset =
      LayoutViewPresetRegistry::GetPreset("layout_mode_view");
  if (!preset)
    return;

  if (defaultLayoutModePerspective.empty()) {
    const std::string previousPerspective =
        auiManager->SavePerspective().ToStdString();

    auto applyPaneState = [this](const std::vector<std::string> &panes,
                                 bool show) {
      for (const auto &name : panes) {
        auto &pane = auiManager->GetPane(name);
        if (pane.IsOk())
          pane.Show(show);
      }
    };

    applyPaneState(preset->showPanes, true);
    applyPaneState(preset->hidePanes, false);
    auiManager->Update();
    defaultLayoutModePerspective = auiManager->SavePerspective().ToStdString();
    auiManager->LoadPerspective(previousPerspective, true);
    auiManager->Update();
  }

  if (defaultLayoutModePerspective.empty())
    ApplyLayoutPreset(*preset, std::nullopt, true, true);
  else
    ApplyLayoutPreset(*preset, std::make_optional(defaultLayoutModePerspective),
                      true, true);
}

void MainWindow::OnApplyDefaultLayout(wxCommandEvent &WXUNUSED(event)) {
  if (!auiManager)
    return;
  if (layoutModeActive)
    PersistLayout2DViewState();
  Ensure3DViewport();
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  std::string perspective = defaultLayoutPerspective;
  if (auto val = cfg.GetValue("layout_default"))
    perspective = *val;

  const auto *preset =
      LayoutViewPresetRegistry::GetPreset("3d_layout_view");
  if (!preset)
    return;
  ApplyLayoutPreset(*preset, std::make_optional(perspective), false, true);
}

void MainWindow::OnApply2DLayout(wxCommandEvent &WXUNUSED(event)) {
  if (!auiManager)
    return;
  if (layoutModeActive)
    PersistLayout2DViewState();
  Ensure2DViewport();
  const auto *preset =
      LayoutViewPresetRegistry::GetPreset("2d_layout_view");
  if (!preset)
    return;
  ApplyLayoutPreset(*preset, std::make_optional(default2DLayoutPerspective),
                    false, true);
}

void MainWindow::OnApplyLayoutModeLayout(wxCommandEvent &WXUNUSED(event)) {
  ApplyLayoutModePerspective();
}

void MainWindow::OnLayoutViewEdit(wxCommandEvent &WXUNUSED(event)) {
  BeginLayout2DViewEdit();
}

void MainWindow::OnLayoutViewSelected(wxCommandEvent &event) {
  if (!layoutModeActive || activeLayoutName.empty() || layout2DViewEditing)
    return;

  const int viewId = event.GetInt();
  if (viewId <= 0)
    return;

  PersistLayout2DViewState();
  RestoreLayout2DViewState(viewId);
}

void MainWindow::OnLayoutAdd2DView(wxCommandEvent &WXUNUSED(event)) {
  if (!layoutModeActive || activeLayoutName.empty())
    return;

  Ensure2DViewport();
  if (!viewport2DPanel)
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  viewer2d::Viewer2DState baseState =
      viewer2d::CaptureState(viewport2DPanel, cfg);
  viewer2d::ApplyEditorRenderOptions(baseState, cfg);
  layouts::Layout2DViewFrame frame = BuildDefaultLayout2DFrame(*layout);
  layouts::Layout2DViewDefinition view =
      viewer2d::ToLayoutDefinition(baseState, frame);

  layouts::LayoutManager::Get().UpdateLayout2DView(activeLayoutName, view);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }
}

void MainWindow::OnLayoutAddLegend(wxCommandEvent &WXUNUSED(event)) {
  if (!layoutModeActive || activeLayoutName.empty())
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  layouts::LayoutLegendDefinition legend;
  legend.frame = BuildDefaultLayoutLegendFrame(*layout);

  layouts::LayoutManager::Get().UpdateLayoutLegend(activeLayoutName, legend);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }
}

void MainWindow::OnLayoutAddEventTable(wxCommandEvent &WXUNUSED(event)) {
  if (!layoutModeActive || activeLayoutName.empty())
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  layouts::LayoutEventTableDefinition table;
  table.frame = BuildDefaultLayoutEventTableFrame(*layout);

  layouts::LayoutManager::Get().UpdateLayoutEventTable(activeLayoutName, table);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }
}

void MainWindow::OnLayoutAddText(wxCommandEvent &WXUNUSED(event)) {
  if (!layoutModeActive || activeLayoutName.empty())
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  layouts::LayoutTextDefinition text;
  text.frame = BuildDefaultLayoutTextFrame(*layout);
  text.text = "Light Plot";
  text.solidBackground = true;
  text.drawFrame = true;

  layouts::LayoutManager::Get().UpdateLayoutText(activeLayoutName, text);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }
}

void MainWindow::OnLayoutAddImage(wxCommandEvent &WXUNUSED(event)) {
  if (!layoutModeActive || activeLayoutName.empty())
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  auto result = PromptForLayoutImage(this, "Selecciona una imagen");
  if (!result)
    return;

  layouts::LayoutImageDefinition image;
  image.frame = BuildDefaultLayoutImageFrame(*layout, result->aspectRatio);
  wxScopedCharBuffer pathBuf = result->path.ToUTF8();
  image.imagePath = pathBuf.data() ? pathBuf.data() : "";
  image.aspectRatio = result->aspectRatio;

  layouts::LayoutManager::Get().UpdateLayoutImage(activeLayoutName, image);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }
}

void MainWindow::BeginLayout2DViewEdit() {
  if (!layoutModeActive || activeLayoutName.empty() || layout2DViewEditing)
    return;

  layout2DViewEditingId = 0;
  Ensure2DViewport();
  if (!viewport2DPanel)
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }

  const layouts::Layout2DViewDefinition *view = nullptr;
  int selectedViewId = 0;
  if (layoutViewerPanel) {
    if (const auto *editableView = layoutViewerPanel->GetEditableView()) {
      selectedViewId = editableView->id;
    }
  }
  if (layout && selectedViewId > 0) {
    view = FindLayout2DViewById(layout, selectedViewId);
  }
  if (!view && layoutViewerPanel)
    view = layoutViewerPanel->GetEditableView();
  if (!view && layout && !layout->view2dViews.empty())
    view = &layout->view2dViews.front();
  if (!view)
    return;
  layout2DViewEditingId = view->id;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();

  Layout2DViewDialog dialog(this);
  layout2DViewEditPanel = dialog.GetViewerPanel();
  layout2DViewEditRenderPanel = dialog.GetRenderPanel();

  Viewer2DPanel *prevPanel = Viewer2DPanel::Instance();
  Viewer2DRenderPanel *prevRenderPanel = Viewer2DRenderPanel::Instance();
  Viewer2DPanel::SetInstance(layout2DViewEditPanel);
  Viewer2DRenderPanel::SetInstance(layout2DViewEditRenderPanel);

  viewer2d::Viewer2DState state = viewer2d::FromLayoutDefinition(*view);
  viewer2d::ApplyEditorRenderOptions(state, cfg);
  layout2DViewStateGuard = std::make_unique<viewer2d::ScopedViewer2DState>(
      layout2DViewEditPanel, layout2DViewEditRenderPanel, cfg, state,
      viewport2DPanel, viewport2DRenderPanel, true);

  if (view->frame.height > 0 && layout2DViewEditPanel) {
    float aspect =
        static_cast<float>(view->frame.width) / view->frame.height;
    std::optional<wxSize> viewportSize;
    if (view->frame.width > 0) {
      viewportSize = wxSize(view->frame.width, view->frame.height);
    }
    layout2DViewEditPanel->SetLayoutEditOverlay(aspect, viewportSize);
  } else if (layout2DViewEditPanel) {
    layout2DViewEditPanel->SetLayoutEditOverlay(std::nullopt);
  }

  layout2DViewEditing = true;
  UpdateViewMenuChecks();

  int result = dialog.ShowModal();
  if (result == wxID_OK) {
    wxCommandEvent evt;
    OnLayout2DViewOk(evt);
  } else {
    wxCommandEvent evt;
    OnLayout2DViewCancel(evt);
  }

  layout2DViewEditPanel = nullptr;
  layout2DViewEditRenderPanel = nullptr;
  Viewer2DPanel::SetInstance(prevPanel);
  Viewer2DRenderPanel::SetInstance(prevRenderPanel);
}

void MainWindow::OnLayout2DViewOk(wxCommandEvent &WXUNUSED(event)) {
  if (!layout2DViewEditing || !layout2DViewStateGuard)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  Viewer2DPanel *editPanel =
      layout2DViewEditPanel ? layout2DViewEditPanel : viewport2DPanel;
  viewer2d::Viewer2DState current =
      viewer2d::CaptureState(editPanel, cfg);
  viewer2d::ApplyEditorRenderOptions(current, cfg);
  const float overlayScale =
      editPanel ? editPanel->GetLayoutEditOverlayScale() : 1.0f;
  if (overlayScale > 0.0f) {
    current.camera.zoom /= overlayScale;
  }

  layouts::Layout2DViewFrame frame{};
  int viewId = layout2DViewEditingId;
  const layouts::Layout2DViewDefinition *editableView = nullptr;
  if (viewId <= 0 && layoutViewerPanel) {
    editableView = layoutViewerPanel->GetEditableView();
    if (editableView)
      viewId = editableView->id;
  }
  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  const layouts::Layout2DViewDefinition *matchedView =
      FindLayout2DViewById(layout, viewId);
  if (matchedView) {
    frame = matchedView->frame;
  } else if (editableView) {
    frame = editableView->frame;
  }

  // Ensure the stored viewport matches the layout frame size, not the popup.
  if (frame.width > 0 || frame.height > 0) {
    current.camera.viewportWidth = frame.width;
    current.camera.viewportHeight = frame.height;
  } else {
    current.camera.viewportWidth = 0;
    current.camera.viewportHeight = 0;
  }

  layouts::Layout2DViewDefinition updatedView =
      viewer2d::ToLayoutDefinition(current, frame);
  updatedView.id = viewId;
  if (matchedView) {
    updatedView.zIndex = matchedView->zIndex;
  } else if (editableView) {
    updatedView.zIndex = editableView->zIndex;
  }
  layouts::LayoutManager::Get().UpdateLayout2DView(activeLayoutName,
                                                   updatedView);

  if (layoutViewerPanel) {
    for (const auto &entry :
         layouts::LayoutManager::Get().GetLayouts().Items()) {
      if (entry.name == activeLayoutName) {
        layoutViewerPanel->SetLayoutDefinition(entry);
        break;
      }
    }
  }

  layout2DViewStateGuard.reset();
  layout2DViewEditingId = 0;

  if (editPanel)
    editPanel->SetLayoutEditOverlay(std::nullopt);

  layout2DViewEditing = false;
  UpdateViewMenuChecks();
}

void MainWindow::OnLayout2DViewCancel(wxCommandEvent &WXUNUSED(event)) {
  if (!layout2DViewEditing || !layout2DViewStateGuard)
    return;

  layout2DViewStateGuard.reset();
  layout2DViewEditingId = 0;

  Viewer2DPanel *editPanel =
      layout2DViewEditPanel ? layout2DViewEditPanel : viewport2DPanel;
  if (editPanel)
    editPanel->SetLayoutEditOverlay(std::nullopt);

  layout2DViewEditing = false;
  UpdateViewMenuChecks();
}

void MainWindow::PersistLayout2DViewState() {
  if (activeLayoutName.empty())
    return;
  if (!layout2DViewEditing)
    return;
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  Viewer2DPanel *activePanel =
      (layout2DViewEditing && layout2DViewEditPanel) ? layout2DViewEditPanel
                                                     : viewport2DPanel;
  if (!activePanel)
    return;
  layouts::Layout2DViewFrame frame{};
  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  int viewId = 0;
  const layouts::Layout2DViewDefinition *editableView = nullptr;
  if (layout2DViewEditing && layout2DViewEditingId > 0) {
    viewId = layout2DViewEditingId;
  } else if (layoutViewerPanel) {
    editableView = layoutViewerPanel->GetEditableView();
    if (editableView)
      viewId = editableView->id;
  }
  const layouts::Layout2DViewDefinition *matchedView =
      FindLayout2DViewById(layout, viewId);
  if (matchedView) {
    frame = matchedView->frame;
  } else if (editableView) {
    frame = editableView->frame;
  }
  layouts::Layout2DViewDefinition view =
      viewer2d::CaptureLayoutDefinition(activePanel, cfg, frame);
  view.id = viewId;
  if (matchedView) {
    view.zIndex = matchedView->zIndex;
  } else if (editableView) {
    view.zIndex = editableView->zIndex;
  }
  layouts::LayoutManager::Get().UpdateLayout2DView(activeLayoutName, view);
}

void MainWindow::RestoreLayout2DViewState(int viewId) {
  if (activeLayoutName.empty())
    return;

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout)
    return;

  const layouts::Layout2DViewDefinition *match =
      FindLayout2DViewById(layout, viewId);
  if (!match)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  if (layoutModeActive && !standalone2DState)
    standalone2DState = viewer2d::CaptureState(nullptr, cfg);
  Viewer2DPanel *activePanel =
      (layout2DViewEditing && layout2DViewEditPanel) ? layout2DViewEditPanel
                                                     : viewport2DPanel;
  Viewer2DRenderPanel *activeRenderPanel =
      (layout2DViewEditing && layout2DViewEditRenderPanel)
          ? layout2DViewEditRenderPanel
          : viewport2DRenderPanel;
  viewer2d::Viewer2DState state = viewer2d::FromLayoutDefinition(*match);
  state.renderOptions.darkMode = cfg.GetFloat("view2d_dark_mode") != 0.0f;
  viewer2d::ApplyState(activePanel, activeRenderPanel, cfg, state, false);
  SyncLayerVisibilityPanels();
}
