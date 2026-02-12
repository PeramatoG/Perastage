#include "fixturetable/fixture_table_parser.h"

#include <iostream>

int main() {
  {
    const auto parts = FixtureTableParser::SplitRangeParts("1 thru 5");
    if (parts.parts.size() != 2 || parts.parts[0] != "1" || parts.parts[1] != "5") {
      std::cerr << "SplitRangeParts failed for thru syntax\n";
      return 1;
    }
  }

  {
    const auto parts = FixtureTableParser::SplitRangeParts("10t");
    if (!parts.usedSeparator || !parts.trailingSeparator || parts.parts.size() != 1 || parts.parts[0] != "10") {
      std::cerr << "SplitRangeParts failed for trailing separator\n";
      return 1;
    }
  }

  {
    const auto addr = FixtureTableParser::ParseAddress("3.125");
    if (addr.universe != 3 || addr.channel != 125) {
      std::cerr << "ParseAddress failed for valid address\n";
      return 1;
    }
  }

  return 0;
}
