#pragma once

#include <string>
#include <wx/arrstr.h>
#include <wx/string.h>

namespace FixtureTableParser {

struct RangeParts {
  wxArrayString parts;
  bool usedSeparator = false;
  bool trailingSeparator = false;
};

RangeParts SplitRangeParts(const wxString &value);

struct ParsedAddress {
  long universe = 0;
  long channel = 0;
};

ParsedAddress ParseAddress(const std::string &address);

} // namespace FixtureTableParser
