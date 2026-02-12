#include "fixture_table_parser.h"

#include <cctype>

namespace {

bool IsNumChar(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
         c == '+';
}

} // namespace

namespace FixtureTableParser {

RangeParts SplitRangeParts(const wxString &value) {
  std::string lower = value.Lower().ToStdString();
  std::string normalized;
  normalized.reserve(lower.size() + 4);
  bool usedSeparator = false;
  bool trailingSeparator = false;
  for (size_t i = 0; i < lower.size();) {
    if (lower.compare(i, 4, "thru") == 0) {
      normalized.push_back(' ');
      usedSeparator = true;
      trailingSeparator = true;
      i += 4;
      continue;
    }
    if (lower[i] == 't') {
      char prev = (i > 0) ? lower[i - 1] : '\0';
      char next = (i + 1 < lower.size()) ? lower[i + 1] : '\0';
      bool standalone =
          (i == 0 || std::isspace(static_cast<unsigned char>(prev))) &&
          (i + 1 >= lower.size() ||
           std::isspace(static_cast<unsigned char>(next)));
      if (standalone || IsNumChar(prev) || IsNumChar(next)) {
        normalized.push_back(' ');
        usedSeparator = true;
        trailingSeparator = true;
        i += 1;
        continue;
      }
    }
    normalized.push_back(lower[i]);
    if (!std::isspace(static_cast<unsigned char>(lower[i])))
      trailingSeparator = false;
    i += 1;
  }
  wxArrayString rawParts = wxSplit(wxString(normalized), ' ');
  wxArrayString parts;
  for (const auto &part : rawParts)
    if (!part.IsEmpty())
      parts.push_back(part);
  return {parts, usedSeparator, trailingSeparator};
}

ParsedAddress ParseAddress(const std::string &address) {
  ParsedAddress parsed;
  if (address.empty())
    return parsed;

  const size_t dot = address.find('.');
  if (dot == std::string::npos)
    return parsed;

  try {
    parsed.universe = std::stol(address.substr(0, dot));
  } catch (...) {
  }
  try {
    parsed.channel = std::stol(address.substr(dot + 1));
  } catch (...) {
  }

  return parsed;
}

} // namespace FixtureTableParser
