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
#include "viewer2dstate.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <wx/choicdlg.h>

#include "configmanager.h"
#include "ui_feature_flags.h"
#include "guiconfigservices.h"
#include "legendsymbolcapture.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "layouttextutils.h"
#include "layoutlegenditems.h"
#include "Viewer2DPrintSettings.h"
#include "print_diagnostics.h"
#include "sceneobjecttablepanel.h"
#include "tableprinter.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpdfexporter.h"
#include "viewer2dprintdialog.h"
#include "LayoutManager.h"

namespace {
void SetPrintStatus(MainWindow *window, const wxString &message) {
  if (!window || !window->GetStatusBar())
    return;
  window->SetStatusText(message, 0);
}

std::vector<LayoutLegendItem> BuildLayoutLegendItems(
    const layouts::LayoutLegendDefinition *legend) {
  std::vector<SharedLayoutLegendItem> sharedItems =
      BuildSharedLayoutLegendItems();
  std::unordered_map<std::string, layouts::LayoutLegendDefinition::ItemSettings>
      settingsByType;
  if (legend) {
    settingsByType.reserve(legend->itemSettings.size());
    for (const auto &settings : legend->itemSettings)
      settingsByType[settings.typeName] = settings;
  }

  std::unordered_map<std::string, SharedLayoutLegendItem> availableByType;
  availableByType.reserve(sharedItems.size());
  for (const auto &shared : sharedItems)
    availableByType.emplace(shared.typeName, shared);

  std::vector<LayoutLegendItem> items;
  items.reserve(sharedItems.size());
  auto appendItem = [&](const SharedLayoutLegendItem &shared) {
    LayoutLegendItem item;
    item.typeName = shared.typeName;
    item.displayName = shared.typeName;
    item.count = shared.count;
    item.channelCount = shared.channelCount;
    item.symbolKey = shared.symbolKey;
    item.symbolFillHex = shared.symbolFillHex;
    if (const auto it = settingsByType.find(shared.typeName);
        it != settingsByType.end()) {
      item.showBottomSymbol = it->second.showBottomSymbol;
      item.showFrontSymbol = it->second.showFrontSymbol;
      item.showSideSymbol = it->second.showSideSymbol;
      if (!it->second.customName.empty())
        item.displayName = it->second.customName;
      if (!it->second.visible)
        return;
    }
    items.push_back(std::move(item));
  };

  if (legend) {
    std::unordered_set<std::string> usedTypes;
    usedTypes.reserve(legend->itemSettings.size());
    for (const auto &settings : legend->itemSettings) {
      const auto it = availableByType.find(settings.typeName);
      if (it == availableByType.end())
        continue;
      appendItem(it->second);
      usedTypes.insert(settings.typeName);
    }
    for (const auto &shared : sharedItems) {
      if (usedTypes.find(shared.typeName) != usedTypes.end())
        continue;
      appendItem(shared);
    }
  } else {
    for (const auto &shared : sharedItems)
      appendItem(shared);
  }

  return items;
}

std::optional<LayoutImageExportData> BuildLayoutImageExportData(
    const layouts::LayoutImageDefinition &imageDef, double scaleX,
    double scaleY) {
  constexpr double kPdfPointsPerInch = 72.0;
  constexpr double kTargetPrintDpi = 300.0;
  constexpr int kMaxImageSidePx = 8192;
  constexpr int kMaxImagePixels = 8192 * 8192;

  LayoutImageExportData data;
  data.zIndex = imageDef.zIndex;
  data.frame = imageDef.frame;
  data.frame.x = static_cast<int>(std::lround(data.frame.x * scaleX));
  data.frame.y = static_cast<int>(std::lround(data.frame.y * scaleY));
  data.frame.width = static_cast<int>(std::lround(data.frame.width * scaleX));
  data.frame.height = static_cast<int>(std::lround(data.frame.height * scaleY));
  if (data.frame.width <= 0 || data.frame.height <= 0)
    return std::nullopt;
  if (imageDef.imagePath.empty())
    return std::nullopt;

  wxImage sourceImage;
  if (!sourceImage.LoadFile(wxString::FromUTF8(imageDef.imagePath)))
    return std::nullopt;
  if (sourceImage.GetWidth() <= 0 || sourceImage.GetHeight() <= 0)
    return std::nullopt;

  const double frameWidthInches =
      static_cast<double>(data.frame.width) / kPdfPointsPerInch;
  const double frameHeightInches =
      static_cast<double>(data.frame.height) / kPdfPointsPerInch;
  int targetPixelWidth = static_cast<int>(
      std::lround(std::max(1.0, frameWidthInches * kTargetPrintDpi)));
  int targetPixelHeight = static_cast<int>(
      std::lround(std::max(1.0, frameHeightInches * kTargetPrintDpi)));
  targetPixelWidth = std::clamp(targetPixelWidth, 1, kMaxImageSidePx);
  targetPixelHeight = std::clamp(targetPixelHeight, 1, kMaxImageSidePx);
  if (targetPixelWidth > 0 &&
      targetPixelHeight > (kMaxImagePixels / targetPixelWidth)) {
    targetPixelHeight = std::max(1, kMaxImagePixels / targetPixelWidth);
  }

  wxImage scaled =
      sourceImage.Scale(targetPixelWidth, targetPixelHeight, wxIMAGE_QUALITY_HIGH);
  if (!scaled.IsOk() || scaled.GetWidth() <= 0 || scaled.GetHeight() <= 0)
    return std::nullopt;

  data.pixelWidth = scaled.GetWidth();
  data.pixelHeight = scaled.GetHeight();
  data.pixels.resize(static_cast<size_t>(data.pixelWidth) *
                     static_cast<size_t>(data.pixelHeight) * 3u);

  const unsigned char *rgb = scaled.GetData();
  const unsigned char *alpha = scaled.HasAlpha() ? scaled.GetAlpha() : nullptr;
  if (!rgb)
    return std::nullopt;
  for (int idx = 0; idx < data.pixelWidth * data.pixelHeight; ++idx) {
    const size_t rgbOffset = static_cast<size_t>(idx) * 3u;
    if (!alpha) {
      data.pixels[rgbOffset + 0] = rgb[rgbOffset + 0];
      data.pixels[rgbOffset + 1] = rgb[rgbOffset + 1];
      data.pixels[rgbOffset + 2] = rgb[rgbOffset + 2];
      continue;
    }

    const double a = static_cast<double>(alpha[idx]) / 255.0;
    data.pixels[rgbOffset + 0] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 0]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
    data.pixels[rgbOffset + 1] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 1]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
    data.pixels[rgbOffset + 2] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 2]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
  }
  return data;
}
}

void MainWindow::OnPrintMenu(wxCommandEvent &WXUNUSED(event)) {
  wxArrayString choices;
  choices.Add("Layout");
  choices.Add("2D View");
  choices.Add("Table");

  wxSingleChoiceDialog dialog(this, "Select what to print:",
                              "Print", choices);
  if (dialog.ShowModal() != wxID_OK)
    return;

  wxCommandEvent printEvent;
  const int selection = dialog.GetSelection();
  if (selection == 0) {
    OnPrintLayout(printEvent);
  } else if (selection == 1) {
    OnPrintViewer2D(printEvent);
  } else {
    OnPrintTable(printEvent);
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

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  ConfigManager *cfgPtr = &cfg;
  print::Viewer2DPrintSettings settings =
      print::Viewer2DPrintSettings::LoadFromConfig(cfg);
  if (ui::IsFeatureEnabled(ui::FeatureFlag::PrintViewer2DDialog)) {
    Viewer2DPrintDialog settingsDialog(this, settings);
    if (settingsDialog.ShowModal() != wxID_OK)
      return;

    settings = settingsDialog.GetSettings();
  }
  ui::ApplyBuildDefaultsToViewer2DPrintSettings(settings);
  settings.includeGrid = true;
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
  SetPrintStatus(this, "Printing 2D view...");

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
          wxLogMessage(wxString::FromUTF8(fixtureReport));
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
              SetPrintStatus(this, "Print failed. Please review the error and try again.");
              wxMessageBox(msg, "Print Viewer 2D", wxOK | wxICON_ERROR, this);
            } else {
              SetPrintStatus(this, "");
              wxMessageBox("2D view saved to " + outputPathDisplay,
                           "Print Viewer 2D", wxOK | wxICON_INFORMATION, this);
            }
          });
        }).detach();
      },
      opts.useSimplifiedFootprints, opts.printIncludeGrid);
}

// Captures layout views and exports them to PDF using the print pipeline settings.
void MainWindow::OnPrintLayout(wxCommandEvent &WXUNUSED(event)) {
  // Flow overview: validate selection and 2D resources, collect settings/file,
  // scale frames to the output, and capture views sequentially before exporting
  // to PDF (ordering guarantees consistent renderer/caches during export).
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
  const bool layoutIsEmpty =
      layout->view2dViews.empty() && layout->legendViews.empty() &&
      layout->eventTables.empty() && layout->textViews.empty() &&
      layout->imageViews.empty();
  if (layoutIsEmpty) {
    wxMessageBox("The selected layout is empty.",
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

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  ConfigManager *cfgPtr = &cfg;
  print::Viewer2DPrintSettings settings =
      print::Viewer2DPrintSettings::LoadFromConfig(cfg);
  settings.pageSize = layout->pageSetup.pageSize;
  settings.landscape = layout->pageSetup.landscape;
  if (ui::IsFeatureEnabled(ui::FeatureFlag::PrintViewer2DDialog)) {
    Viewer2DPrintDialog settingsDialog(this, settings, false);
    if (settingsDialog.ShowModal() != wxID_OK)
      return;

    settings = settingsDialog.GetSettings();
  }
  settings.landscape = layout->pageSetup.landscape;
  ui::ApplyBuildDefaultsToViewer2DPrintSettings(settings);
  settings.includeGrid = true;
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

  const bool useSimplifiedFootprints = false;
  const bool includeGrid = true;
  std::vector<layouts::Layout2DViewDefinition> layoutViews =
      layout->view2dViews;
  std::vector<LayoutLegendExportData> layoutLegends;
  layoutLegends.reserve(layout->legendViews.size());
  std::vector<LayoutEventTableExportData> layoutTables;
  layoutTables.reserve(layout->eventTables.size());
  std::vector<LayoutTextExportData> layoutTexts;
  layoutTexts.reserve(layout->textViews.size());
  std::vector<LayoutImageExportData> layoutImages;
  layoutImages.reserve(layout->imageViews.size());
  for (const auto &legend : layout->legendViews) {
    LayoutLegendExportData legendData;
    legendData.items = BuildLayoutLegendItems(&legend);
    legendData.zIndex = legend.zIndex;
    legendData.showChannelColumn = legend.showChannelColumn;
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
  for (const auto &image : layout->imageViews) {
    auto imageData = BuildLayoutImageExportData(image, scaleX, scaleY);
    if (imageData.has_value())
      layoutImages.push_back(std::move(*imageData));
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
  auto exportImages =
      std::make_shared<std::vector<LayoutImageExportData>>(
          std::move(layoutImages));

  if (capturePanel)
    capturePanel->SetPreferPerastageSvgSymbolsForLayouts(true);

  auto captureNext =
      std::make_shared<std::function<void(size_t)>>();
  *captureNext =
      [this, captureNext, exportViews, layoutViews, offscreenRenderer,
       capturePanel, cfgPtr, useSimplifiedFootprints, includeGrid, scaleX,
       scaleY, outputPageW, outputPageH, outputLandscape, exportLegends,
       exportTables, exportTexts, exportImages, outputPathWx](size_t index) mutable {
        if (index >= layoutViews.size()) {
          Viewer2DPrintOptions opts;
          opts.pageWidthPt = outputPageW;
          opts.pageHeightPt = outputPageH;
          opts.marginPt = 0.0;
          opts.layoutScaleX = scaleX;
          opts.layoutScaleY = scaleY;
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
          auto imagesToExport = std::move(*exportImages);
          if (capturePanel) {
            auto legendSymbols =
                CaptureLegendSymbolSnapshot(capturePanel, *cfgPtr, true);
            for (auto &legend : legendsToExport) {
              legend.symbolSnapshot = legendSymbols;
            }
          }

          std::thread([this, views = std::move(viewsToExport), opts,
                       legends = std::move(legendsToExport),
                       tables = std::move(tablesToExport),
                       texts = std::move(textsToExport),
                       images = std::move(imagesToExport), outputPath,
                       outputPathDisplay]() {
            Viewer2DExportResult res =
                ExportLayoutToPdf(views, legends, tables, texts, images, opts,
                                  outputPath);

            wxTheApp->CallAfter([this, res, outputPathDisplay]() {
              if (!res.success) {
                wxString msg = "Failed to generate layout PDF: " +
                               wxString::FromUTF8(res.message);
                SetPrintStatus(this,
                               "Print failed. Please review the error and try again.");
                wxMessageBox(msg, "Print Layout", wxOK | wxICON_ERROR, this);
              } else {
                SetPrintStatus(this, "");
                wxString successMessage = "Layout saved to " + outputPathDisplay;
                wxMessageBox(successMessage, "Print Layout",
                             wxOK | wxICON_INFORMATION, this);
              }
            });
          }).detach();
          if (capturePanel)
            capturePanel->SetPreferPerastageSvgSymbolsForLayouts(false);
          return;
        }

        const auto &view = layoutViews[index];
        viewer2d::Viewer2DState layoutState =
            viewer2d::FromLayoutDefinition(view);
        viewer2d::ApplyEditorRenderOptions(layoutState, *cfgPtr);
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
            capturePanel, nullptr, *cfgPtr, layoutState, nullptr, nullptr,
            false);
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
              data.drawFrame = view.drawFrame;
              if (capturePanel)
                data.symbolSnapshot =
                    capturePanel->GetBottomSymbolCacheSnapshot();
              exportViews->push_back(std::move(data));
              (*captureNext)(exportViews->size());
            },
            useSimplifiedFootprints, includeGrid);
      };

  SetPrintStatus(this, "Printing layout...");
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
    TablePrinter::Print(this, ctrl, type, GetDefaultGuiConfigServices().LegacyConfigManager());
}
