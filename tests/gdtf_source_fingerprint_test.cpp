#include "gdtf_source_fingerprint.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// Splits a fingerprint into its delimiter-separated fields.
std::vector<std::string> SplitFingerprint(const std::string &fingerprint) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= fingerprint.size()) {
    const std::size_t next = fingerprint.find('|', start);
    if (next == std::string::npos) {
      parts.push_back(fingerprint.substr(start));
      break;
    }
    parts.push_back(fingerprint.substr(start, next - start));
    start = next + 1;
  }
  return parts;
}

// Writes test content to a temporary file.
void WriteFile(const fs::path &path, const std::string &content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
}

// Verifies GDTF source fingerprints use path, size, and normalized timestamp fields.
int main() {
  const fs::path dir = fs::temp_directory_path() / "perastage_gdtf_source_fingerprint_test_dir";
  if (fs::exists(dir) && !fs::is_directory(dir))
    fs::remove(dir);
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path file = dir / "fixture.gdtf";
  WriteFile(file, "first");

  const std::string initial = gui::BuildGdtfSourceFingerprint(file);
  assert(!initial.empty());
  const auto initialParts = SplitFingerprint(initial);
  assert(initialParts.size() == 3);
  assert(!initialParts[0].empty());
  assert(initialParts[1] == "5");
  assert(!initialParts[2].empty());

  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  WriteFile(file, "second content");
  const std::string updated = gui::BuildGdtfSourceFingerprint(file);
  assert(updated != initial);
  const auto updatedParts = SplitFingerprint(updated);
  assert(updatedParts.size() == 3);
  assert(updatedParts[1] == "14");

  const fs::path missing = dir / "missing.gdtf";
  const std::string missingFingerprint = gui::BuildGdtfSourceFingerprint(missing);
  const auto missingParts = SplitFingerprint(missingFingerprint);
  assert(missingParts.size() == 3);
  assert(missingParts[1] == "0");
  assert(missingParts[2] == "0");

  fs::remove(file);
  fs::remove_all(dir);
  return 0;
}
