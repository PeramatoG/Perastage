#include "mvr_export_file_buffer_parity_test_support.h"

#include "configmanager.h"
#include "layer.h"
#include "mvrexporter.h"

#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cassert>
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
    assert(entries.emplace(entry->GetName().ToStdString(), ReadEntry(zip)).second);
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

  wxFileInputStream fileInput(filePath.generic_string());
  assert(fileInput.IsOk());
  const auto fileEntries = ReadEntries(fileInput);
  wxMemoryInputStream bufferInput(buffer.data(), buffer.size());
  assert(fileEntries == ReadEntries(bufferInput));
}
