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
#pragma once

#include "json.hpp"

#include <optional>
#include <string>

namespace DictionaryJsonContract {

inline constexpr const char *kKind = "perastage.dictionary";
inline constexpr int kSchemaVersion = 1;

struct ParsedDictionary {
  std::string dictionaryType;
  const nlohmann::json *entries = nullptr;
};

inline bool IsSupportedDictionaryType(const std::string &value) {
  return value == "fixtures" || value == "trusses" || value == "combined";
}

inline bool Parse(const nlohmann::json &root, ParsedDictionary &out,
                  std::string &error) {
  if (!root.is_object()) {
    error = "root must be a JSON object";
    return false;
  }

  const auto kindIt = root.find("kind");
  if (kindIt == root.end() || !kindIt->is_string()) {
    error = "missing or invalid 'kind' (expected string 'perastage.dictionary')";
    return false;
  }
  if (kindIt->get<std::string>() != kKind) {
    error = "invalid 'kind' (expected 'perastage.dictionary')";
    return false;
  }

  const auto schemaVersionIt = root.find("schema_version");
  if (schemaVersionIt == root.end() || !schemaVersionIt->is_number_integer()) {
    error = "missing or invalid 'schema_version' (expected integer)";
    return false;
  }
  if (schemaVersionIt->get<int>() != kSchemaVersion) {
    error = "unsupported 'schema_version' (expected 1)";
    return false;
  }

  const auto dictionaryTypeIt = root.find("dictionary_type");
  if (dictionaryTypeIt == root.end() || !dictionaryTypeIt->is_string()) {
    error = "missing or invalid 'dictionary_type' (expected fixtures/trusses/combined)";
    return false;
  }

  const std::string dictionaryType = dictionaryTypeIt->get<std::string>();
  if (!IsSupportedDictionaryType(dictionaryType)) {
    error = "invalid 'dictionary_type' (expected fixtures/trusses/combined)";
    return false;
  }

  const auto entriesIt = root.find("entries");
  if (entriesIt == root.end()) {
    error = "missing required 'entries'";
    return false;
  }
  if (!entriesIt->is_object() && !entriesIt->is_array()) {
    error = "invalid 'entries' (expected object or array)";
    return false;
  }

  out.dictionaryType = dictionaryType;
  out.entries = &(*entriesIt);
  return true;
}

inline std::optional<const nlohmann::json *>
GetEntriesForType(const nlohmann::json &root, const std::string &expectedType,
                  std::string &error) {
  ParsedDictionary parsed;
  if (!Parse(root, parsed, error))
    return std::nullopt;

  if (parsed.dictionaryType == "combined") {
    if (!parsed.entries->is_object()) {
      error = "combined dictionaries require 'entries' to be an object";
      return std::nullopt;
    }
    const auto childIt = parsed.entries->find(expectedType);
    if (childIt == parsed.entries->end()) {
      error = "combined dictionary is missing entries for '" + expectedType + "'";
      return std::nullopt;
    }
    if (!childIt->is_object() && !childIt->is_array()) {
      error = "combined entries for '" + expectedType + "' must be object or array";
      return std::nullopt;
    }
    return &(*childIt);
  }

  if (parsed.dictionaryType != expectedType) {
    error = "dictionary_type mismatch (expected '" + expectedType + "')";
    return std::nullopt;
  }

  return parsed.entries;
}

inline nlohmann::json MakeRoot(const std::string &dictionaryType,
                               nlohmann::json entries) {
  nlohmann::json root = nlohmann::json::object();
  root["kind"] = kKind;
  root["schema_version"] = kSchemaVersion;
  root["dictionary_type"] = dictionaryType;
  root["entries"] = std::move(entries);
  return root;
}

} // namespace DictionaryJsonContract
