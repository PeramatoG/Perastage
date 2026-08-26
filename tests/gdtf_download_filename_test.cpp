#include "gdtf_download_filename.h"

#include <cassert>
#include <fstream>

// Verifies readable deterministic names and collision-safe revision suffixes.
int main() {
  using namespace gdtf_download_filename;
  assert(BuildReadableFileName("GLP", "JDC1") == "GLP JDC1.gdtf");
  assert(BuildReadableFileName("GLP", "JDC1") ==
         BuildReadableFileName("GLP", "JDC1"));
  assert(BuildReadableFileName("Maker/Unsafe", "Fixture:*?") ==
         "Maker_Unsafe Fixture___.gdtf");
  assert(BuildReadableFileName("", "CON") == "_CON.gdtf");

  const auto directory = std::filesystem::temp_directory_path() /
                         "perastage-gdtf-download-filename-test";
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  std::filesystem::create_directories(directory, error);
  assert(!error);
  const auto readable = ChooseDestination(directory, "GLP", "JDC1", "123456");
  assert(readable.filename() == "GLP JDC1.gdtf");
  assert(readable.filename() != "123456.gdtf");
  std::ofstream(readable).put('\n');
  const auto collision =
      ChooseDestination(directory, "GLP", "JDC1", "123456");
  assert(collision.filename() == "GLP JDC1 - 123456.gdtf");
  assert(collision ==
         ChooseDestination(directory, "GLP", "JDC1", "123456"));
  std::filesystem::remove_all(directory, error);
  return 0;
}
