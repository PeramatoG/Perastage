/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_export_transport.h"

#include "runtime_storage.h"

#include <filesystem>
#include <fstream>

namespace mvr_export_transport {
namespace fs = std::filesystem;

// Writes an archive to temporary storage and returns its bytes.
Result WriteToBuffer(const FileWriter &writer, std::vector<uint8_t> &outBytes) {
  outBytes.clear();
  runtime_storage::TemporaryWorkspace workspace("mvr-export-buffer");
  if (!workspace.IsValid())
    return {false, false, "CreateWorkspace", "export-buffer.mvr", {},
            "MVR export could not create its temporary archive workspace."};

  const fs::path archivePath = workspace.Path() / "export-buffer.mvr";
  if (!writer(archivePath.string()))
    return {false, true};

  std::error_code error;
  const auto size = fs::exists(archivePath, error) && !error
                        ? fs::file_size(archivePath, error)
                        : 0;
  if (error || size == 0)
    return {false, false, "ReadBufferArchive", "export-buffer.mvr",
            archivePath.string(),
            "MVR export-to-buffer produced an empty or unreadable file '" +
                archivePath.string() + "' size=" + std::to_string(size)};

  std::ifstream input(archivePath, std::ios::binary);
  if (!input.is_open())
    return {false, false, "ReadBufferArchive", "export-buffer.mvr",
            archivePath.string(),
            "MVR export-to-buffer could not open " + archivePath.string()};
  input.seekg(0, std::ios::end);
  const std::streampos readSize = input.tellg();
  input.seekg(0, std::ios::beg);
  if (readSize <= 0)
    return {false, false, "ReadBufferArchive", "export-buffer.mvr",
            archivePath.string(),
            "MVR export-to-buffer read zero bytes from " + archivePath.string()};
  outBytes.resize(static_cast<size_t>(readSize));
  input.read(reinterpret_cast<char *>(outBytes.data()), readSize);
  if ((!input.good() && !input.eof()) || outBytes.empty())
    return {false, false, "ReadBufferArchive", "export-buffer.mvr",
            archivePath.string(),
            "MVR export-to-buffer failed to read payload from " +
                archivePath.string()};
  return {true};
}

} // namespace mvr_export_transport
