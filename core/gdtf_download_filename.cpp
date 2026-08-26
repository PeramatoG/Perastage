#include "gdtf_download_filename.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace gdtf_download_filename {
namespace {

// Replaces characters forbidden or unsafe in portable local filenames.
std::string SanitizeComponent(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  bool previousSpace = false;
  for (const char character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    const bool forbidden = byte < 32 || character == '<' || character == '>' ||
                           character == ':' || character == '"' ||
                           character == '/' || character == '\\' ||
                           character == '|' || character == '?' ||
                           character == '*';
    const char output = forbidden ? '_' : character;
    const bool isSpace = std::isspace(static_cast<unsigned char>(output)) != 0;
    if (isSpace && previousSpace)
      continue;
    result.push_back(isSpace ? ' ' : output);
    previousSpace = isSpace;
  }
  while (!result.empty() &&
         (result.front() == ' ' || result.front() == '.'))
    result.erase(result.begin());
  while (!result.empty() &&
         (result.back() == ' ' || result.back() == '.'))
    result.pop_back();
  return result;
}

// Protects Windows device names while remaining harmless on other platforms.
std::string ProtectReservedStem(std::string stem) {
  std::string upper = stem;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](char character) {
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(character)));
  });
  static const std::string reserved[] = {
      "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  if (std::find(std::begin(reserved), std::end(reserved), upper) !=
      std::end(reserved))
    stem.insert(stem.begin(), '_');
  return stem;
}

// Returns a portable revision suffix used only when the readable name collides.
std::string RevisionSuffix(const std::string &revisionId) {
  std::string suffix;
  for (const char character : revisionId) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (std::isalnum(byte) || character == '-' || character == '_')
      suffix.push_back(character);
  }
  return suffix.empty() ? "revision" : suffix;
}

} // namespace

// Builds a portable human-readable filename from authoritative catalog identity.
std::string BuildReadableFileName(const std::string &manufacturer,
                                  const std::string &fixtureName) {
  const std::string safeManufacturer = SanitizeComponent(manufacturer);
  const std::string safeFixture = SanitizeComponent(fixtureName);
  std::string stem;
  if (!safeManufacturer.empty() && !safeFixture.empty())
    stem = safeManufacturer + " " + safeFixture;
  else if (!safeFixture.empty())
    stem = safeFixture;
  else if (!safeManufacturer.empty())
    stem = safeManufacturer;
  else
    stem = "GDTF Share fixture";
  return ProtectReservedStem(stem) + ".gdtf";
}

// Chooses the readable name first and adds a stable revision only on collision.
std::filesystem::path ChooseDestination(
    const std::filesystem::path &directory, const std::string &manufacturer,
    const std::string &fixtureName, const std::string &revisionId) {
  const std::filesystem::path readable =
      directory / BuildReadableFileName(manufacturer, fixtureName);
  std::error_code error;
  if (!std::filesystem::exists(readable, error))
    return readable;
  const std::string stem = readable.stem().string();
  return directory / (stem + " - " + RevisionSuffix(revisionId) + ".gdtf");
}

} // namespace gdtf_download_filename
