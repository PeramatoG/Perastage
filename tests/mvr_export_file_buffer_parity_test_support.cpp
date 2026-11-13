#include "mvr_export_file_buffer_parity_test_support.h"

#include "configmanager.h"
#include "LayoutManager.h"
#include "LayoutTemplateSerializer.h"
#include "layer.h"
#include "mvrexporter.h"
#include "wx_path_utils.h"

#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Reads the active ZIP entry payload.
static std::string ReadEntry(wxZipInputStream &zip) {
  std::string payload;
  char buffer[4096];
  while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0)
    payload.append(buffer, zip.LastRead());
  return payload;
}

// Reads every named payload from an MVR stream.
static std::unordered_map<std::string, std::string>
ReadEntries(wxInputStream &input) {
  wxZipInputStream zip(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry)
    assert(entries
               .emplace(entry->GetName(wxPATH_UNIX).ToStdString(), ReadEntry(zip))
               .second);
  return entries;
}

// Verifies semantic package parity between public file and buffer exports.
void VerifyMvrExportFileBufferParity(ConfigManager &config,
                                     const fs::path &directory) {
  config.Reset();
  MvrScene &scene = config.GetScene();
  scene.basePath = directory.generic_string();
  Layer layer;
  layer.uuid = "30000000-0000-4000-8000-000000000001";
  layer.name = "Parity Layer";
  scene.layers[layer.uuid] = layer;

  const fs::path filePath = directory / "file-buffer-parity.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(filePath.generic_string()));
  std::vector<uint8_t> buffer;
  assert(exporter.ExportToBuffer(buffer));

  wxFileInputStream fileInput(
      WxPathUtils::WxStringFromFilesystemPath(filePath));
  assert(fileInput.IsOk());
  const auto fileEntries = ReadEntries(fileInput);
  wxMemoryInputStream bufferInput(buffer.data(), buffer.size());
  assert(fileEntries == ReadEntries(bufferInput));

  // Layout physical settings are native presentation metadata. Exercise the
  // production PSTG path and prove they do not enter canonical scene.mvr.
  layouts::LayoutDefinition page; page.name="Physical parity";
  layouts::Layout2DViewDefinition view; view.id=42;
  layouts::SetViewportFrame(view,{17.123,29.456,101.234,73.345});
  view.scaleMode=layouts::ViewportScaleMode::Fixed;
  view.physicalScale=layouts::PhysicalViewport{50,2,-3};
  page.view2dViews.push_back(view);
  view.id=43; view.physicalScale=layouts::PhysicalViewport{100,-8,9};
  page.view2dViews.push_back(view);
  assert(layouts::LayoutManager::Get().AddLayout(page));
  assert(exporter.ExportToBuffer(buffer));
  wxMemoryInputStream withLayout(buffer.data(),buffer.size());
  assert(fileEntries==ReadEntries(withLayout));
  const auto nativePath=directory/"physical-parity.pstg";
  assert(config.SaveProject(nativePath.generic_string()));
  wxFileInputStream native(WxPathUtils::WxStringFromFilesystemPath(nativePath));
  const auto nativeEntries=ReadEntries(native);
  const auto &sceneBytes=nativeEntries.at("scene.mvr");
  wxMemoryInputStream nativeScene(sceneBytes.data(),sceneBytes.size());
  assert(fileEntries==ReadEntries(nativeScene));
  config.Reset();
  assert(config.LoadProject(nativePath.generic_string()));
  const auto &pages=layouts::LayoutManager::Get().GetLayouts().Items();
  const auto restored=std::find_if(pages.begin(),pages.end(),[&](const auto &p) { return p.name==page.name; });
  assert(restored!=pages.end() && layouts::ToJson(*restored)==layouts::ToJson(page));

}
