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

#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include <wx/filename.h>

#include "configmanager.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "hoisttablepanel.h"
#include "layouttextutils.h"
#include "legendutils.h"
#include "print/Viewer2DPrintSettings.h"
#include "print_diagnostics.h"
#include "sceneobjecttablepanel.h"
#include "tableprinter.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpdfexporter.h"
#include "viewer2dprintdialog.h"
#include "layouts/LayoutManager.h"

namespace {
std::vector<LayoutLegendItem> BuildLayoutLegendItems() {
  struct LegendAggregate {
    int count = 0;
    std::optional<int> channelCount;
    bool mixedChannels = false;
    std::string symbolKey;
    bool mixedSymbols = false;
  };

  std::map<std::string, LegendAggregate> aggregates;
  const auto &fixtures = ConfigManager::Get().GetScene().fixtures;
  const std::string &basePath = ConfigManager::Get().GetScene().basePath;
  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    std::string typeName = fixture.typeName;
    std::string fullPath;
    if (!fixture.gdtfSpec.empty()) {
      std::filesystem::path p = basePath.empty()
                                    ? std::filesystem::path(fixture.gdtfSpec)
                                    : std::filesystem::path(basePath) /
                                          fixture.gdtfSpec;
      fullPath = p.string();
    }
    if (typeName.empty() && !fullPath.empty()) {
      wxFileName fn(fullPath);
      typeName = fn.GetFullName().ToStdString();
    }
    if (typeName.empty())
      typeName = "Unknown";

    int chCount = GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
    const std::string symbolKey = BuildFixtureSymbolKey(fixture, basePath);
    LegendAggregate &agg = aggregates[typeName];
    agg.count += 1;
    if (chCount >= 0) {
      if (!agg.channelCount.has_value()) {
        agg.channelCount = chCount;
      } else if (agg.channelCount.value() != chCount) {
        agg.mixedChannels = true;
      }
    }
    if (!symbolKey.empty()) {
      if (agg.symbolKey.empty()) {
        agg.symbolKey = symbolKey;
      } else if (agg.symbolKey != symbolKey) {
        agg.mixedSymbols = true;
      }
    }
  }

  std::vector<LayoutLegendItem> items;
  items.reserve(aggregates.size());
  for (const auto &[typeName, agg] : aggregates) {
    LayoutLegendItem item;
    item.typeName = typeName;
    item.count = agg.count;
    if (agg.channelCount.has_value() && !agg.mixedChannels)
      item.channelCount = agg.channelCount;
    if (!agg.mixedSymbols)
      item.symbolKey = agg.symbolKey;
    items.push_back(item);
  }

  if (items.empty()) {
    LayoutLegendItem item;
    item.typeName = "No fixtures";
    item.count = 0;
    items.push_back(item);
  }

  return items;
}
}

void MainWindow::OnPrintViewer2D(wxCommandEvent &WXUNUSED(event)) {
  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  Viewer2DPanel *capturePanel =
      offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  if (!capturePanel) {
    wxMessageBox("2D viewport is not available.", "Print Viewer 2D",
                 wxOK | wxICON_ERROR);
    return;
  }

  ConfigManager &cfg = ConfigManager::Get();
  ConfigManager *cfgPtr = &cfg;
  print::Viewer2DPrintSettings settings =
      print::Viewer2DPrintSettings::LoadFromConfig(cfg);
  Viewer2DPrintDialog settingsDialog(this, settings);
  if (settingsDialog.ShowModal() != wxID_OK)
    return;

  settings = settingsDialog.GetSettings();
  settings.SaveToConfig(cfg);

  wxFileDialog dlg(this, "Save 2D view as", "", "viewer2d.pdf",
                   "PDF files (*.pdf)|*.pdf",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString outputPathWx = dlg.GetPath();
  outputPathWx.Trim(true).Trim(false);
  if (outputPathWx.empty()) {
    wxMessageBox("Please choose a destination file for the 2D view.",
                 "Print Viewer 2D", wxOK | wxICON_WARNING);
    return;
  }

  Viewer2DPrintOptions opts; // Defaults to A3 portrait.
  opts.landscape = settings.landscape;
  opts.printIncludeGrid = settings.includeGrid;
  opts.useSimplifiedFootprints = !settings.detailedFootprints;
  opts.pageWidthPt = settings.PageWidthPt();
  opts.pageHeightPt = settings.PageHeightPt();
  std::filesystem::path outputPath(
      std::filesystem::path(outputPathWx.ToStdWstring()));
  wxString outputPathDisplay = outputPathWx;

  wxSize captureSize = viewport2DPanel ? viewport2DPanel->GetClientSize()
                                       : GetClientSize();
  if (captureSize.GetWidth() <= 0 || captureSize.GetHeight() <= 0) {
    captureSize = wxSize(1600, 900);
  }
  if (viewport2DPanel)
    viewport2DPanel->SaveViewToConfig();
  offscreenRenderer->SetViewportSize(captureSize);
  offscreenRenderer->PrepareForCapture();

  capturePanel->CaptureFrameNow(
      [this, capturePanel, opts, outputPath, outputPathDisplay](
          CommandBuffer buffer, Viewer2DViewState state) {
        if (buffer.commands.empty()) {
          wxMessageBox("Unable to capture the 2D view for printing.",
                       "Print Viewer 2D", wxOK | wxICON_ERROR);
          return;
        }

        std::string diagnostics = BuildPrintDiagnostics(buffer);
        if (ConsolePanel::Instance()) {
          ConsolePanel::Instance()->AppendMessage(
              wxString::FromUTF8(diagnostics));
        }

        std::string fixtureReport;
        if (capturePanel)
          fixtureReport = capturePanel->GetLastFixtureDebugReport();
        if (!fixtureReport.empty()) {
          wxLogMessage("%s", wxString::FromUTF8(fixtureReport));
          if (ConsolePanel::Instance()) {
            ConsolePanel::Instance()->AppendMessage(
                wxString::FromUTF8(fixtureReport));
          }
        }

        std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot = nullptr;
        if (capturePanel) {
          symbolSnapshot = capturePanel->GetBottomSymbolCacheSnapshot();
        }

        // Run the PDF generation off the UI thread to avoid freezing the
        // window while writing potentially large plans to disk.
        std::thread([this, buffer = std::move(buffer), state, opts, outputPath,
                     outputPathDisplay, symbolSnapshot]() {
          Viewer2DExportResult res = ExportViewer2DToPdf(
              buffer, state, opts, outputPath, symbolSnapshot);

          wxTheApp->CallAfter([this, res, outputPathDisplay]() {
            if (!res.success) {
              wxString msg = "Failed to generate PDF plan: " +
                             wxString::FromUTF8(res.message);
              wxMessageBox(msg, "Print Viewer 2D", wxOK | wxICON_ERROR, this);
            } else {
              wxMessageBox(wxString::Format("2D view saved to %s",
                                            outputPathDisplay),
                           "Print Viewer 2D", wxOK | wxICON_INFORMATION, this);
            }
          });
        }).detach();
      },
      opts.useSimplifiedFootprints, opts.printIncludeGrid);
}

void MainWindow::OnPrintLayout(wxCommandEvent &WXUNUSED(event)) {
  if (activeLayoutName.empty()) {
    wxMessageBox("No layout is selected.", "Print Layout", wxOK | wxICON_WARNING,
                 this);
    return;
  }

  const layouts::LayoutDefinition *layout = nullptr;
  for (const auto &entry : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (entry.name == activeLayoutName) {
      layout = &entry;
      break;
    }
  }
  if (!layout) {
    wxMessageBox("Selected layout is not available.", "Print Layout", wxOK,
                 this);
    return;
  }
  if (layout->view2dViews.empty()) {
    wxMessageBox("The selected layout has no 2D views to print.",
                 "Print Layout", wxOK | wxICON_INFORMATION, this);
    return;
  }

  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  Viewer2DPanel *capturePanel =
      offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  if (!capturePanel) {
    wxMessageBox("2D viewport is not available.", "Print Layout", wxOK,
                 this);
    return;
  }

  ConfigManager &cfg = ConfigManager::Get();
  ConfigManager *cfgPtr = &cfg;
  print::Viewer2DPrintSettings settings =
      print::Viewer2DPrintSettings::LoadFromConfig(cfg);
  settings.pageSize = layout->pageSetup.pageSize;
  settings.landscape = layout->pageSetup.landscape;
  Viewer2DPrintDialog settingsDialog(this, settings, false);
  if (settingsDialog.ShowModal() != wxID_OK)
    return;

  settings = settingsDialog.GetSettings();
  settings.landscape = layout->pageSetup.landscape;
  settings.SaveToConfig(cfg);

  wxFileDialog dlg(this, "Save layout as", "", "layout.pdf",
                   "PDF files (*.pdf)|*.pdf",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString outputPathWx = dlg.GetPath();
  outputPathWx.Trim(true).Trim(false);
  if (outputPathWx.empty()) {
    wxMessageBox("Please choose a destination file for the layout.",
                 "Print Layout", wxOK | wxICON_WARNING, this);
    return;
  }

  print::PageSetup outputSetup = settings;
  outputSetup.landscape = layout->pageSetup.landscape;
  const double outputPageW = outputSetup.PageWidthPt();
  const double outputPageH = outputSetup.PageHeightPt();
  const bool outputLandscape = outputSetup.landscape;
  const double layoutPageW = layout->pageSetup.PageWidthPt();
  const double layoutPageH = layout->pageSetup.PageHeightPt();
  const double scaleX =
      layoutPageW > 0.0 ? outputPageW / layoutPageW : 1.0;
  const double scaleY =
      layoutPageH > 0.0 ? outputPageH / layoutPageH : 1.0;

  const bool useSimplifiedFootprints = !settings.detailedFootprints;
  const bool includeGrid = settings.includeGrid;
  std::vector<layouts::Layout2DViewDefinition> layoutViews =
      layout->view2dViews;
  std::vector<LayoutLegendExportData> layoutLegends;
  layoutLegends.reserve(layout->legendViews.size());
  std::vector<LayoutEventTableExportData> layoutTables;
  layoutTables.reserve(layout->eventTables.size());
  std::vector<LayoutTextExportData> layoutTexts;
  layoutTexts.reserve(layout->textViews.size());
  const auto legendItems = BuildLayoutLegendItems();
  for (const auto &legend : layout->legendViews) {
    LayoutLegendExportData legendData;
    legendData.items = legendItems;
    legendData.zIndex = legend.zIndex;
    layouts::Layout2DViewFrame frame = legend.frame;
    frame.x = static_cast<int>(std::lround(frame.x * scaleX));
    frame.y = static_cast<int>(std::lround(frame.y * scaleY));
    frame.width = static_cast<int>(std::lround(frame.width * scaleX));
    frame.height = static_cast<int>(std::lround(frame.height * scaleY));
    legendData.frame = frame;
    layoutLegends.push_back(std::move(legendData));
  }
  for (const auto &table : layout->eventTables) {
    LayoutEventTableExportData tableData;
    tableData.fields = table.fields;
    tableData.zIndex = table.zIndex;
    layouts::Layout2DViewFrame frame = table.frame;
    frame.x = static_cast<int>(std::lround(frame.x * scaleX));
    frame.y = static_cast<int>(std::lround(frame.y * scaleY));
    frame.width = static_cast<int>(std::lround(frame.width * scaleX));
    frame.height = static_cast<int>(std::lround(frame.height * scaleY));
    tableData.frame = frame;
    layoutTables.push_back(std::move(tableData));
  }
  for (const auto &text : layout->textViews) {
    layoutTexts.push_back(
        layouttext::BuildLayoutTextExportData(text, scaleX, scaleY));
  }
  auto exportViews = std::make_shared<std::vector<LayoutViewExportData>>();
  exportViews->reserve(layoutViews.size());
  auto exportLegends =
      std::make_shared<std::vector<LayoutLegendExportData>>(
          std::move(layoutLegends));
  auto exportTables =
      std::make_shared<std::vector<LayoutEventTableExportData>>(
          std::move(layoutTables));
  auto exportTexts =
      std::make_shared<std::vector<LayoutTextExportData>>(
          std::move(layoutTexts));

  auto captureNext =
      std::make_shared<std::function<void(size_t)>>();
  *captureNext =
      [this, captureNext, exportViews, layoutViews, offscreenRenderer,
       capturePanel, cfgPtr, useSimplifiedFootprints, includeGrid, scaleX,
       scaleY, outputPageW, outputPageH, outputLandscape, exportLegends,
       exportTables, exportTexts, outputPathWx](size_t index) mutable {
        if (index >= layoutViews.size()) {
          Viewer2DPrintOptions opts;
          opts.pageWidthPt = outputPageW;
          opts.pageHeightPt = outputPageH;
          opts.marginPt = 0.0;
          opts.landscape = outputLandscape;
          opts.printIncludeGrid = includeGrid;
          opts.useSimplifiedFootprints = useSimplifiedFootprints;
          std::filesystem::path outputPath(
              std::filesystem::path(outputPathWx.ToStdWstring()));
          wxString outputPathDisplay = outputPathWx;
          auto viewsToExport = std::move(*exportViews);
          auto legendsToExport = std::move(*exportLegends);
          auto tablesToExport = std::move(*exportTables);
          auto textsToExport = std::move(*exportTexts);
          if (capturePanel) {
            auto legendSymbols = capturePanel->GetBottomSymbolCacheSnapshot();
            for (auto &legend : legendsToExport) {
              legend.symbolSnapshot = legendSymbols;
            }
          }

          std::thread([this, views = std::move(viewsToExport), opts,
                       legends = std::move(legendsToExport),
                       tables = std::move(tablesToExport),
                       texts = std::move(textsToExport), outputPath,
                       outputPathDisplay]() {
            Viewer2DExportResult res =
                ExportLayoutToPdf(views, legends, tables, texts, opts,
                                  outputPath);

            wxTheApp->CallAfter([this, res, outputPathDisplay]() {
              if (!res.success) {
                wxString msg = "Failed to generate layout PDF: " +
                               wxString::FromUTF8(res.message);
                wxMessageBox(msg, "Print Layout", wxOK | wxICON_ERROR, this);
              } else {
                wxMessageBox(wxString::Format("Layout saved to %s",
                                              outputPathDisplay),
                             "Print Layout", wxOK | wxICON_INFORMATION, this);
              }
            });
          }).detach();
          return;
        }

        const auto &view = layoutViews[index];
        viewer2d::Viewer2DState layoutState =
            viewer2d::FromLayoutDefinition(view);
        layoutState.renderOptions.darkMode = false;

        const int fallbackViewportWidth = view.camera.viewportWidth > 0
                                              ? view.camera.viewportWidth
                                              : view.frame.width;
        const int fallbackViewportHeight = view.camera.viewportHeight > 0
                                               ? view.camera.viewportHeight
                                               : view.frame.height;
        const int viewportWidth =
            fallbackViewportWidth > 0 ? fallbackViewportWidth : 1600;
        const int viewportHeight =
            fallbackViewportHeight > 0 ? fallbackViewportHeight : 900;

        if (offscreenRenderer && viewportWidth > 0 && viewportHeight > 0) {
          offscreenRenderer->SetViewportSize(
              wxSize(viewportWidth, viewportHeight));
          offscreenRenderer->PrepareForCapture();
        }

        auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
            capturePanel, nullptr, *cfgPtr, layoutState);
        capturePanel->CaptureFrameNow(
            [captureNext, exportViews, view, viewportWidth, viewportHeight,
             capturePanel, scaleX, scaleY,
             stateGuard](CommandBuffer buffer, Viewer2DViewState state) {
              LayoutViewExportData data;
              data.buffer = std::move(buffer);
              data.viewState = state;
              if (data.viewState.viewportWidth <= 0)
                data.viewState.viewportWidth = viewportWidth;
              if (data.viewState.viewportHeight <= 0)
                data.viewState.viewportHeight = viewportHeight;
              layouts::Layout2DViewFrame frame = view.frame;
              frame.x =
                  static_cast<int>(std::lround(frame.x * scaleX));
              frame.y =
                  static_cast<int>(std::lround(frame.y * scaleY));
              frame.width =
                  static_cast<int>(std::lround(frame.width * scaleX));
              frame.height =
                  static_cast<int>(std::lround(frame.height * scaleY));
              data.frame = frame;
              data.zIndex = view.zIndex;
              if (capturePanel)
                data.symbolSnapshot =
                    capturePanel->GetBottomSymbolCacheSnapshot();
              exportViews->push_back(std::move(data));
              (*captureNext)(exportViews->size());
            },
            useSimplifiedFootprints, includeGrid);
      };

  (*captureNext)(0);
}

void MainWindow::OnPrintTable(wxCommandEvent &WXUNUSED(event)) {
  wxArrayString options;
  if (fixturePanel)
    options.Add("Fixtures");
  if (trussPanel)
    options.Add("Trusses");
  if (hoistPanel)
    options.Add("Hoists");
  if (sceneObjPanel)
    options.Add("Objects");
  if (options.IsEmpty())
    return;

  wxSingleChoiceDialog dlg(this, "Select table", "Print Table", options);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString choice = dlg.GetStringSelection();
  wxDataViewListCtrl *ctrl = nullptr;
  TablePrinter::TableType type = TablePrinter::TableType::Fixtures;
  if (choice == "Fixtures" && fixturePanel) {
    ctrl = fixturePanel->GetTableCtrl();
    type = TablePrinter::TableType::Fixtures;
  } else if (choice == "Trusses" && trussPanel) {
    ctrl = trussPanel->GetTableCtrl();
    type = TablePrinter::TableType::Trusses;
  } else if (choice == "Hoists" && hoistPanel) {
    ctrl = hoistPanel->GetTableCtrl();
    type = TablePrinter::TableType::Supports;
  } else if (choice == "Objects" && sceneObjPanel) {
    ctrl = sceneObjPanel->GetTableCtrl();
    type = TablePrinter::TableType::SceneObjects;
  }

  if (ctrl)
    TablePrinter::Print(this, ctrl, type);
}
