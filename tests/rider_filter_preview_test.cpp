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
      "CALLES A SUELO:\n"
      "4 LITELEE B-EYE L10R\n"
      "EFECTOS\n"
      "4 TOUR HAZER II + TURBINA\n"
      "GROUND LANES:\n"
      "2 LED BAR\n"
      "RIGGING Y ESTRUCTURAS\n"
      "3 MOTOR 500Kg + ESLINGAS PARA PUENTES LX\n"
      "2 MOTOR 1TO + ESLINGAS PARA PA\n"
      "3 TRUSS 40X40 PRO 14m PARA PUENTES LX\n"
      "1 TRUSS 40X40 PRO 14m PARA PUENTE PANTALLA\n";

  const std::string expected =
      "LX1\n"
      "8 PROLIGHT BLINDER 4 PRO\n"
      "8 S-K15 LIROLITE\n"
      "\n"
      "\n"
      "FLOOR\n"
      "4 LITELEE B-EYE L10R\n"
      "4 TOUR HAZER II\n"
      "4 TURBINA\n"
      "2 LED BAR\n"
      "\n"
      "\n"
      "RIGGING\n"
      "1 MOTOR 500Kg PARA LX1\n"
      "1 MOTOR 500Kg PARA LX2\n"
      "1 MOTOR 500Kg PARA LX3\n"
      "2 MOTOR 1000Kg PARA P.A.\n"
      "1 TRUSS 40X40 PRO 14m LX1\n"
      "1 TRUSS 40X40 PRO 14m LX2\n"
      "1 TRUSS 40X40 PRO 14m LX3\n"
      "1 TRUSS 40X40 PRO 14m SCREEN";

  const std::string preview = RiderImporter::BuildFixtureFilterPreview(input);
  if (preview != expected) {
    std::cerr << "Unexpected filtered preview output.\n"
              << "Expected:\n"
              << expected << "\n\nGot:\n"
              << preview << "\n";
    return 1;
  }

  const std::string previewSecondPass =
      RiderImporter::BuildFixtureFilterPreview(preview);
  if (previewSecondPass != expected) {
    std::cerr << "Filtered preview is not idempotent.\n"
              << "Expected:\n"
              << expected << "\n\nGot after second pass:\n"
              << previewSecondPass << "\n";
    return 1;
  }

  return 0;
}
