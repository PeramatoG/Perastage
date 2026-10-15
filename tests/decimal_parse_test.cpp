#include "decimal_parse.h"
#include "gdtf_power_reader.h"

#include <cassert>
#include <locale>
#include <string>
#include <string_view>

namespace {
struct CommaDecimal : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};

template <typename Float> void CheckDecimalParsing() {
  Float value = 7;
  size_t consumed = 0;
  assert(perastage::numeric::ParseDecimalPrefix("-.125e+2Z", value, consumed));
  assert(value == static_cast<Float>(-12.5) && consumed == 8);
  assert(perastage::numeric::ParseDecimalPrefix("1eZ", value, consumed));
  assert(value == 1 && consumed == 1);
  for (std::string_view text : {"", "-", ".", "+1", " 1", "nan", "inf",
                                "1e9999", "1e-9999"}) {
    value = 7;
    assert(!perastage::numeric::ParseDecimalPrefix(text, value, consumed));
    assert(value == 7 && consumed == 0);
  }
  assert(perastage::numeric::ParseDecimalPrefix("0e-9999", value, consumed));
  assert(value == 0 && consumed == 7);
}

void CheckPower() {
  for (std::string_view text : {"0", "-0", "+0", "  +12.5e1\t", ".5", "5."})
    assert(gdtf::ParseNonnegativePower(text));
  assert(gdtf::ParseNonnegativePower("  +12.5e1\t") == 125);
  for (std::string_view text : {"", "+", "++1", "+ 1", "-1", "nan", "inf",
                                "1e9999", "1e-9999", "1e", "1W", "0x10",
                                "1 2", "1,5", "\v1"})
    assert(!gdtf::ParseNonnegativePower(text));
  assert(!gdtf::ParseNonnegativePower(std::string("1\0junk", 6)));
}
} // namespace

int main() {
  CheckDecimalParsing<float>();
  CheckDecimalParsing<double>();
  CheckDecimalParsing<long double>();
  CheckPower();
  const std::locale previous = std::locale();
  std::locale::global(std::locale(previous, new CommaDecimal));
  CheckDecimalParsing<float>();
  CheckDecimalParsing<double>();
  CheckPower();
  std::locale::global(previous);
}
