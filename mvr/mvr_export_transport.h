/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mvr_export_transport {

struct Result {
  bool success{false};
  bool archiveWriteFailed{false};
  std::string operation;
  std::string archivePath;
  std::string sourcePath;
  std::string reason;
};

using FileWriter = std::function<bool(const std::string &)>;

// Writes an archive to temporary storage and returns its bytes.
Result WriteToBuffer(const FileWriter &writer, std::vector<uint8_t> &outBytes);

} // namespace mvr_export_transport
