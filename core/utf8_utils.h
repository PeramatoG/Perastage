#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

struct Utf8ValidationResult {
  bool valid = true;
  size_t errorOffset = 0;
};

Utf8ValidationResult ValidateUtf8(std::string_view text);
bool IsValidUtf8(std::string_view text);
std::optional<std::string> RepairWindows1252AsUtf8(std::string_view text);
std::string EscapeTextForDiagnostics(std::string_view text);
