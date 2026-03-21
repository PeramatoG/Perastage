#include "riderimporter.h"

#include <iostream>
#include <string>

int main() {
  const std::string input =
      "Rider Tecnico\n"
      "ILUMINACION\n"
      "LX1:\n"
      "8 PROLIGHT BLINDER 4 PRO\n"
      "8 S-K15 LIROLITE (LIBRERIA K10)\n"
      "SONIDO\n"
      "1 YAMAHA CL5\n"
      "ILUMINACION\n"
      "EFECTOS\n"
      "4 TOUR HAZER II + TURBINA\n";

  const std::string expected =
      "LX1\n"
      "8 PROLIGHT BLINDER 4 PRO\n"
      "8 S-K15 LIROLITE\n"
      "\n"
      "\n"
      "FLOOR\n"
      "4 TOUR HAZER II\n"
      "4 TURBINA";

  const std::string preview = RiderImporter::BuildFixtureFilterPreview(input);
  if (preview != expected) {
    std::cerr << "Unexpected filtered preview output.\n"
              << "Expected:\n"
              << expected << "\n\nGot:\n"
              << preview << "\n";
    return 1;
  }

  return 0;
}
