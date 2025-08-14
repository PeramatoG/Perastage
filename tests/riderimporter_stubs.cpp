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
#include <string>
#include <optional>
#include <unordered_map>

#include "../core/gdtfdictionary.h"
#include "../core/trussdictionary.h"
#include "../core/trussloader.h"
#include "../core/pdftext.h"
#include "../mvr/mvrimporter.h"
#include "../mvr/mvrexporter.h"
#include "../models/truss.h"

std::string ExtractPdfText(const std::string &) { return {}; }

namespace GdtfDictionary {
std::optional<std::unordered_map<std::string, Entry>> Load() { return std::unordered_map<std::string, Entry>(); }
void Save(const std::unordered_map<std::string, Entry> &) {}
std::optional<Entry> Get(const std::string &) { return std::nullopt; }
void Update(const std::string &, const std::string &, const std::string &) {}
}

namespace TrussDictionary {
std::optional<std::unordered_map<std::string, std::string>> Load() { return std::unordered_map<std::string, std::string>(); }
void Save(const std::unordered_map<std::string, std::string> &) {}
std::optional<std::string> Get(const std::string &) { return std::nullopt; }
void Update(const std::string &, const std::string &) {}
}

bool LoadTrussArchive(const std::string &, Truss &) { return false; }

bool MvrImporter::ImportFromFile(const std::string &, bool, bool) { return false; }
bool MvrImporter::ImportAndRegister(const std::string &, bool, bool) { return false; }
bool MvrExporter::ExportToFile(const std::string &) { return false; }
