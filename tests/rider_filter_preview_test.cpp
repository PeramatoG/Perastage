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

  const std::string inputWithCoordinates =
      "LX1 (6)\n"
      "2 SPOT\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m PARA LX1 (6)\n";
  const std::string previewWithCoordinates =
      RiderImporter::BuildFixtureFilterPreview(inputWithCoordinates);
  const std::string expectedWithCoordinates =
      "LX1 (6)\n"
      "2 SPOT\n"
      "\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m LX1 (6)";
  if (previewWithCoordinates != expectedWithCoordinates) {
    std::cerr << "Coordinate overrides should be preserved in filtered preview.\n"
              << "Expected:\n"
              << expectedWithCoordinates << "\n\nGot:\n"
              << previewWithCoordinates << "\n";
    return 1;
  }

  const std::string inputWithoutParaCoordinates =
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m LX1 (-1)\n"
      "1 TRUSS 40X40 BACKDROP (8)\n";
  const std::string previewWithoutParaCoordinates =
      RiderImporter::BuildFixtureFilterPreview(inputWithoutParaCoordinates);
  const std::string expectedWithoutParaCoordinates =
      "\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m LX1 (-1)\n"
      "1 TRUSS 40X40 BACKDROP (8)";
  if (previewWithoutParaCoordinates != expectedWithoutParaCoordinates) {
    std::cerr << "No-PARA truss targets with coordinates should be preserved.\n"
              << "Expected:\n"
              << expectedWithoutParaCoordinates << "\n\nGot:\n"
              << previewWithoutParaCoordinates << "\n";
    return 1;
  }

  const std::string inputEnglishForKeyword =
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m for LX2 (4)\n"
      "1 TRUSS 40X40 for BACKDROP (8)\n";
  const std::string previewEnglishForKeyword =
      RiderImporter::BuildFixtureFilterPreview(inputEnglishForKeyword);
  const std::string expectedEnglishForKeyword =
      "\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m LX2 (4)\n"
      "1 TRUSS 40X40 BACKDROP (8)";
  if (previewEnglishForKeyword != expectedEnglishForKeyword) {
    std::cerr << "English 'for' truss targets should be preserved.\n"
              << "Expected:\n"
              << expectedEnglishForKeyword << "\n\nGot:\n"
              << previewEnglishForKeyword << "\n";
    return 1;
  }

  const std::string mixedSectionsInput =
      "SONIDO\n"
      "2 CLUSTER FORMADOS POR:\n"
      "ILUMINACION\n"
      "LX1:\n"
      "4 SPOT\n";
  const std::string mixedSectionsExpected =
      "LX1\n"
      "4 SPOT";
  const std::string mixedSectionsPreview =
      RiderImporter::BuildFixtureFilterPreview(mixedSectionsInput);
  if (mixedSectionsPreview != mixedSectionsExpected) {
    std::cerr << "Audio sections should not leak into fixture preview when "
                 "using headerless FLOOR fallback.\n"
              << "Expected:\n"
              << mixedSectionsExpected << "\n\nGot:\n"
              << mixedSectionsPreview << "\n";
    return 1;
  }

  return 0;
}
