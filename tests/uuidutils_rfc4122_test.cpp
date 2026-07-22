#include "uuidutils.h"

#include <cassert>
#include <regex>
#include <string>

int main() {
  const std::regex uuid4Pattern(
      "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
  const std::string generated = GenerateUuid();
  assert(std::regex_match(generated, uuid4Pattern));
  assert(IsValidUuid(generated));
  assert(CanonicalizeUuid("{not-a-uuid}").empty());
  assert(CanonicalizeUuid("76CAF3D74C4C2C5535BEAB47E26D86B3") ==
         "76caf3d7-4c4c-2c55-35be-ab47e26d86b3");
  return 0;
}
