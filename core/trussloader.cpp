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
#include "trussloader.h"
#include "../external/json.hpp"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <memory>

using nlohmann::json;

bool LoadTrussArchive(const std::string &archivePath, Truss &outTruss) {
  namespace fs = std::filesystem;
  wxFileInputStream input(archivePath);
  if (!input.IsOk())
    return false;
  outTruss.modelFile = archivePath;
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string meta;
  fs::path baseDir = fs::temp_directory_path();
  std::string uniqueName = "perastage-truss-";
  {
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 15);
    for (int i = 0; i < 6; ++i)
      uniqueName += hex[dist(rd)];
  }
  baseDir /= uniqueName;
  wxFileName::Mkdir(baseDir.string(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
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

