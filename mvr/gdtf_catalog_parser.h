#pragma once

#include "gdtf_catalog_matcher.h"

#include <string>
#include <vector>

namespace mvr::gdtf_catalog_parser {

// Parses a GDTF Share getList.php response while retaining legacy cache aliases.
std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
ParseCatalogEntries(const std::string &payload);

} // namespace mvr::gdtf_catalog_parser
