#include "riderimporter.h"

#include "configmanager.h"

#include <cassert>
#include <iostream>
#include <string>
#include <wx/init.h>

// Reports whether a preview follows the canonical serialization contract.
bool HasCanonicalSerialization(const std::string &preview) {
  if (preview.empty() || preview.front() == '\n' || preview.back() == '\n' ||
      preview.find('\r') != std::string::npos ||
      preview.find("\n\n\n") != std::string::npos)
    return false;

  size_t lineStart = 0;
  while (lineStart < preview.size()) {
    const size_t lineEnd = preview.find('\n', lineStart);
    const size_t end = lineEnd == std::string::npos ? preview.size() : lineEnd;
    if (end > lineStart &&
        (preview[end - 1] == ' ' || preview[end - 1] == '\t'))
      return false;
    if (lineEnd == std::string::npos)
      break;
    lineStart = lineEnd + 1;
  }
  return true;
}

// Converts LF input to CRLF input for line-ending normalization coverage.
std::string ToCrLf(const std::string &input) {
  std::string result;
  result.reserve(input.size() + input.size() / 8);
  for (const char character : input) {
    if (character == '\n')
      result.push_back('\r');
    result.push_back(character);
  }
  return result;
}

// Verifies rider filter preview stability and import parity.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

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
      "FLOOR\n"
      "4 LITELEE B-EYE L10R\n"
      "2 LED BAR\n"
      "\n"
      "RIGGING\n"
      "1 MOTOR 500Kg FOR LX1\n"
      "1 MOTOR 500Kg FOR LX2\n"
      "1 MOTOR 500Kg FOR LX3\n"
      "2 MOTOR 1000Kg FOR P.A.\n"
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
  assert(HasCanonicalSerialization(preview));

  const auto rawRequests = RiderImporter::AnalyzeFixtureTypes(input, false);
  const auto filteredRequests = RiderImporter::AnalyzeFixtureTypes(preview, true);
  assert(rawRequests.size() == 4);
  assert(filteredRequests.size() == rawRequests.size());
  for (size_t i = 0; i < rawRequests.size(); ++i) {
    assert(rawRequests[i].typeName == filteredRequests[i].typeName);
    assert(rawRequests[i].quantity == filteredRequests[i].quantity);
    assert(rawRequests[i].positions == filteredRequests[i].positions);
  }
  const auto aggregated = RiderImporter::AnalyzeFixtureTypes(
      "LX1:\n8 GLP JDC1\nLX2:\n4 GLP JDC1\n", false);
  assert(aggregated.size() == 1);
  assert(aggregated.front().quantity == 12);
  assert((aggregated.front().positions ==
          std::vector<std::string>{"LX1", "LX2"}));

  const std::string crlfPreview =
      RiderImporter::BuildFixtureFilterPreview(ToCrLf(input));
  assert(crlfPreview == preview);
  assert(HasCanonicalSerialization(crlfPreview));

  const std::string previewSecondPass =
      RiderImporter::BuildFixtureFilterPreview(preview);
  if (previewSecondPass != expected) {
    std::cerr << "Filtered preview is not idempotent.\n"
              << "Expected:\n"
              << expected << "\n\nGot after second pass:\n"
              << previewSecondPass << "\n";
    return 1;
  }
  assert(previewSecondPass == preview);

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
      "RIGGING\n"
      "1 TRUSS 40X40 14m LX1 (6)";
  if (previewWithCoordinates != expectedWithCoordinates) {
    std::cerr << "Coordinate overrides should be preserved in filtered preview.\n"
              << "Expected:\n"
              << expectedWithCoordinates << "\n\nGot:\n"
              << previewWithCoordinates << "\n";
    return 1;
  }
  assert(HasCanonicalSerialization(previewWithCoordinates));

  const std::string inputWithMarginOverride =
      "LX1 [0.8]\n"
      "2 SPOT\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m PARA LX1 [0.8]\n";
  const std::string previewWithMarginOverride =
      RiderImporter::BuildFixtureFilterPreview(inputWithMarginOverride);
  const std::string expectedWithMarginOverride =
      "LX1 [0.8]\n"
      "2 SPOT\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m LX1 [0.8]";
  if (previewWithMarginOverride != expectedWithMarginOverride) {
    std::cerr << "Margin overrides should be preserved in filtered preview.\n"
              << "Expected:\n"
              << expectedWithMarginOverride << "\n\nGot:\n"
              << previewWithMarginOverride << "\n";
    return 1;
  }
  assert(HasCanonicalSerialization(previewWithMarginOverride));

  const std::string inputWithoutParaCoordinates =
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m LX1 (-1)\n"
      "1 TRUSS 40X40 BACKDROP (8)\n";
  const std::string previewWithoutParaCoordinates =
      RiderImporter::BuildFixtureFilterPreview(inputWithoutParaCoordinates);
  const std::string expectedWithoutParaCoordinates =
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
  assert(HasCanonicalSerialization(previewWithoutParaCoordinates));

  const std::string inputEnglishForKeyword =
      "RIGGING\n"
      "1 TRUSS 40X40 PRO NEGRO 12m for LX2 (4)\n"
      "1 TRUSS 40X40 for BACKDROP (8)\n";
  const std::string previewEnglishForKeyword =
      RiderImporter::BuildFixtureFilterPreview(inputEnglishForKeyword);
  const std::string expectedEnglishForKeyword =
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
  assert(HasCanonicalSerialization(previewEnglishForKeyword));

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
  assert(HasCanonicalSerialization(mixedSectionsPreview));

  auto assertImportParity = [](const std::string &riderText) {
    auto &cfg = ConfigManager::Get();
    const std::string filtered = RiderImporter::BuildFixtureFilterPreview(riderText);

    cfg.Reset();
    assert(RiderImporter::ImportText(riderText));
    const auto directFixtureCount = cfg.GetScene().fixtures.size();
    const auto directTrussCount = cfg.GetScene().trusses.size();
    const auto directSupportCount = cfg.GetScene().supports.size();
    const auto directSceneObjectCount = cfg.GetScene().sceneObjects.size();

    cfg.Reset();
    assert(RiderImporter::ImportText(filtered));
    const auto filteredFixtureCount = cfg.GetScene().fixtures.size();
    const auto filteredTrussCount = cfg.GetScene().trusses.size();
    const auto filteredSupportCount = cfg.GetScene().supports.size();
    const auto filteredSceneObjectCount = cfg.GetScene().sceneObjects.size();

    assert(directFixtureCount == filteredFixtureCount);
    assert(directTrussCount == filteredTrussCount);
    assert(directSupportCount == filteredSupportCount);
    assert(directSceneObjectCount == filteredSceneObjectCount);
  };

  assertImportParity(input);
  assertImportParity(inputWithCoordinates);
  assertImportParity(
      "ILUMINACION\n"
      "SCREEN\n"
      "1 PANTALLA LED 8X5m 1664X1040 PIXELS\n"
      "RIGGING\n"
      "1 PIPE 12m PARA PANTALLA\n");

  const std::string spanishRegression =
      "ILUMINACIÓN\nAPARATOS\nFRONTAL:\n8 BLINDER\nCENITAL:\n6 WASH\n"
      "CONTRA:\n8 PROFILE\nCALLES DIRECTAS A LAYHER:\n8 LED BAR\n"
      "SUELO:\n4 BEAM\n1 FOLLOWSPOT + OPERADOR\n"
      "4 PAR LED PARA ILUMINAR ESCALERA\nEFECTOS\n2 HAZER + TURBINA\n"
      "CONTROL DE ILUMINACION\n1 GRAND MA3\nVIDEO\nPANTALLA LED\n"
      "1 PANTALLA LED 10X5m 1664x832 PIXELS\nRIGGING Y ESTRUCTURAS\n"
      "4 MOTOR 2TO + GRILLETES + ESLINGAS PARA PA\n"
      "3 TRUSS 40X40 PRO 12m PARA PUENTES LX\n"
      "6 MOTOR 500Kg + GRILLETES + ESLINGAS PARA PUENTES LX\n"
      "1 TRUSS 40X40 PRO 12m PARA PUENTE PANTALLA\n"
      "4 MOTOR 1TO + GRILLETES + ESLINGAS PARA PUENTE PANTALLA\n";
  const std::string spanishExpected =
      "LX1\n8 BLINDER\n\nLX2\n6 WASH\n\nLX3\n8 PROFILE\n\n"
      "LX SIDES\n8 LED BAR\n\nFLOOR\n4 BEAM\n1 FOLLOWSPOT\n1 OPERADOR\n"
      "4 PAR LED PARA ILUMINAR ESCALERA\n\n"
      "SCREEN\n1 PANTALLA LED 10X5m 1664x832 PIXELS\n\nRIGGING\n"
      "4 MOTOR 2000Kg FOR P.A.\n2 MOTOR 500Kg FOR LX1\n"
      "2 MOTOR 500Kg FOR LX2\n2 MOTOR 500Kg FOR LX3\n"
      "4 MOTOR 1000Kg FOR SCREEN\n1 TRUSS 40X40 PRO 12m LX1\n"
      "1 TRUSS 40X40 PRO 12m LX2\n1 TRUSS 40X40 PRO 12m LX3\n"
      "1 TRUSS 40X40 PRO 12m SCREEN";
  assert(RiderImporter::BuildFixtureFilterPreview(spanishRegression) ==
         spanishExpected);
  assert(RiderImporter::BuildFixtureFilterPreview(spanishExpected) ==
         spanishExpected);
  assertImportParity(spanishRegression);

  const std::string englishRegression =
      "LIGHTING\nFIXTURES\nFRONT:\n8 BLINDER\nMID:\n6 WASH\nBACK:\n"
      "8 PROFILE\nSIDES:\n8 LED BAR\nFLOOR:\n4 BEAM\nEFFECTS\n2 HAZER\n"
      "LIGHTING CONTROL\n1 GRANDMA3\nVIDEO\nLED WALL\n"
      "1 LED WALL 10 x 5 m\n";
  const std::string englishExpected =
      "LX1\n8 BLINDER\n\nLX2\n6 WASH\n\nLX3\n8 PROFILE\n\n"
      "LX SIDES\n8 LED BAR\n\nFLOOR\n4 BEAM\n\n"
      "SCREEN\n1 LED WALL 10 x 5 m";
  assert(RiderImporter::BuildFixtureFilterPreview(englishRegression) ==
         englishExpected);
  assertImportParity(englishRegression);

  const std::string keywordFixtureRegression =
      "FLOOR\n1 FIXTURE VIDEO CONTROL AUDIO LIGHTING\n2 HAZER\n";
  assert(RiderImporter::BuildFixtureFilterPreview(keywordFixtureRegression) ==
         "FLOOR\n1 FIXTURE VIDEO CONTROL AUDIO LIGHTING\n2 HAZER");

  std::string longHeadingNoiseRegression;
  for (int index = 0; index < 250; ++index) {
    longHeadingNoiseRegression +=
        "Technical rider notes with formatting words but no section meaning ";
    longHeadingNoiseRegression.append(120, 'X');
    longHeadingNoiseRegression += "\n";
  }
  longHeadingNoiseRegression += "LIGHTING\nFRONT\n2 SPOT\n";
  assert(RiderImporter::BuildFixtureFilterPreview(longHeadingNoiseRegression) ==
         "LX1\n2 SPOT");

  return 0;
}
