#include "fixture_patch_address.h"

#include <cassert>
#include <limits>
#include <string>

// Characterizes the strict canonical stored fixture patch-address grammar.
int main() {
  using perastage::patch::FixturePatchAddress;
  using perastage::patch::ParseFixturePatchAddress;

  assert(ParseFixturePatchAddress("1.1") == (FixturePatchAddress{1, 1}));
  assert(ParseFixturePatchAddress("23.512") == (FixturePatchAddress{23, 512}));
  assert(!ParseFixturePatchAddress(""));
  assert(!ParseFixturePatchAddress("1"));
  assert(!ParseFixturePatchAddress(".1"));
  assert(!ParseFixturePatchAddress("1."));
  assert(!ParseFixturePatchAddress("1.2.3"));
  assert(!ParseFixturePatchAddress(" 1.2"));
  assert(!ParseFixturePatchAddress("1.2 "));
  assert(!ParseFixturePatchAddress("1 .2"));
  assert(!ParseFixturePatchAddress("1.2channels"));
  assert(!ParseFixturePatchAddress("0.1"));
  assert(!ParseFixturePatchAddress("1.0"));
  assert(!ParseFixturePatchAddress("1.513"));
  assert(!ParseFixturePatchAddress("999999999999999999999999.1"));
  assert(!ParseFixturePatchAddress("1.999999999999999999999999"));
  return 0;
}
