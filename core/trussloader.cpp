#include "trussloader.h"
#include "../external/json.hpp"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>
#include <filesystem>
#include <fstream>
#include <memory>

using nlohmann::json;

bool LoadTrussArchive(const std::string &archivePath, Truss &outTruss) {
  namespace fs = std::filesystem;
  wxFileInputStream input(archivePath);
  if (!input.IsOk())
    return false;
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string meta;
  fs::path baseDir = fs::path(archivePath).parent_path();
  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string name = entry->GetName().ToStdString();
    if (entry->IsDir())
      continue;
    if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") {
      std::string contents;
      char buf[4096];
      while (true) {
        zip.Read(buf, sizeof(buf));
        size_t bytes = zip.LastRead();
        if (bytes == 0)
          break;
        contents.append(buf, bytes);
      }
      meta = std::move(contents);
    } else if (name.size() >= 4 &&
               (name.substr(name.size() - 4) == ".3ds" ||
                name.substr(name.size() - 4) == ".glb")) {
      fs::path dest = baseDir / fs::path(name).filename();
      wxFileName::Mkdir(dest.parent_path().string(), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      std::ofstream out(dest, std::ios::binary);
      if (!out.is_open())
        return false;
      char buf[4096];
      while (true) {
        zip.Read(buf, sizeof(buf));
        size_t bytes = zip.LastRead();
        if (bytes == 0)
          break;
        out.write(buf, bytes);
      }
      out.close();
      outTruss.symbolFile = dest.string();
    }
  }
  if (meta.empty() || outTruss.symbolFile.empty())
    return false;
  json j = json::parse(meta, nullptr, false);
  if (j.is_discarded())
    return false;
  outTruss.name = j.value("Name", "");
  outTruss.manufacturer = j.value("Manufacturer", "");
  outTruss.model = j.value("Model", "");
  outTruss.lengthMm = j.value("Length_mm", 0.0f);
  outTruss.widthMm = j.value("Width_mm", 0.0f);
  outTruss.heightMm = j.value("Height_mm", 0.0f);
  outTruss.weightKg = j.value("Weight_kg", 0.0f);
  outTruss.crossSection = j.value("CrossSection", "");
  return true;
}

