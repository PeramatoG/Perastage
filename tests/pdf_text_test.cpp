#include "pdftext.h"
#include "support/pdf_test_fixture_builder.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

// Reads and normalizes the trailing line terminators in expected text files.
std::string ReadExpected(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
  while (!text.empty() && (text.back() == '\n' || text.back() == '\f'))
    text.pop_back();
  return text;
}

// Compares one structurally valid fixture using the diagnostic extraction API.
bool CheckFixture(const std::filesystem::path &path) {
  std::string bytes;
  std::string error;
  if (!pdf_test_support::ReadBinaryFile(path, bytes, error) ||
      !pdf_test_support::ValidatePdfStructure(bytes, error)) {
    std::cerr << "Fixture integrity failed for " << path << ": " << error << '\n';
    return false;
  }
  const auto result = ExtractPdfTextWithResult(path.string());
  if (!result.success) {
    std::cerr << "Extraction failed for " << path << ": " << result.error << '\n';
    return false;
  }
  const std::filesystem::path expectedPath = path.parent_path() /
      (path.stem().string() + ".txt");
  const std::string expected = ReadExpected(expectedPath);
  if (result.text != expected) {
    std::cerr << "Mismatch for " << path << "\nExpected:\n" << expected
              << "\nActual:\n" << result.text << '\n';
    return false;
  }
  return true;
}

// Verifies positioned reconstruction without depending on PDF parser behavior.
bool CheckReconstruction() {
  auto fragment = [](std::string text, double x, double y, double advance) {
    return PdfTextFragment{std::move(text), x, y, advance};
  };
  const std::vector<std::pair<std::vector<PdfTextFragment>, std::string>> cases = {
      {{fragment("Hello World", 0, 20, 60)}, "Hello World"},
      {{fragment("Foo", 0, 20, 18), fragment("Bar", 68, 20, 18)}, "Foo Bar"},
      {{fragment("Foo", 0, 20, 18), fragment("Bar", 18.5, 20, 18)}, "FooBar"},
      {{fragment("Foo ", 0, 20, 18), fragment("Bar", 68, 20, 18)}, "Foo Bar"},
      {{fragment("Second", 0, 10, 30), fragment("First", 0, 20, 25)},
       "First\nSecond"}};
  for (const auto &[fragments, expected] : cases) {
    const std::string actual = ReconstructPdfText(fragments);
    if (actual != expected) {
      std::cerr << "Reconstruction mismatch: expected '" << expected
                << "', actual '" << actual << "'\n";
      return false;
    }
  }
  return true;
}

// Verifies empty, malformed, missing, and multi-page diagnostic contracts.
bool CheckDiagnosticContracts() {
  const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("perastage_pdf_text_" + std::to_string(unique));
  std::error_code cleanupError;
  std::filesystem::create_directories(directory, cleanupError);
  if (cleanupError) {
    std::cerr << "Unable to create PDF text temporary directory: "
              << cleanupError.message() << '\n';
    return false;
  }
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup{directory};

  std::string error;
  const auto emptyPath = directory / "empty.pdf";
  const auto pagesPath = directory / "pages.pdf";
  const auto malformedPath = directory / "malformed.pdf";
  if (!pdf_test_support::WriteBinaryFile(emptyPath, pdf_test_support::BuildPdf({""}), error) ||
      !pdf_test_support::WriteBinaryFile(
          pagesPath, pdf_test_support::BuildPdf({"BT\n/F1 12 Tf\n10 20 Td\n(One) Tj\nET\n",
                                                  "BT\n/F1 12 Tf\n10 20 Td\n(Two) Tj\nET\n"}), error) ||
      !pdf_test_support::WriteBinaryFile(malformedPath, "%PDF-1.4\ninvalid", error)) {
    std::cerr << error << '\n';
    return false;
  }
  const auto empty = ExtractPdfTextWithResult(emptyPath.string());
  const auto pages = ExtractPdfTextWithResult(pagesPath.string());
  const auto malformed = ExtractPdfTextWithResult(malformedPath.string());
  const auto missing = ExtractPdfTextWithResult((directory / "missing.pdf").string());
  if (!empty.success || !empty.text.empty() || !pages.success ||
      pages.text != "One\nTwo" || malformed.success || malformed.error.empty() ||
      missing.success || missing.error.find("Unable to read") == std::string::npos) {
    std::cerr << "PDF extraction result contract failed. Empty error: " << empty.error
              << "; pages: " << pages.text << "; malformed: " << malformed.error
              << "; missing: " << missing.error << '\n';
    return false;
  }
  return true;
}

} // namespace

// Runs fixture integrity, reconstruction, and extraction diagnostics controls.
int main(int argc, char **argv) {
  if (argc < 2 || !CheckReconstruction() || !CheckDiagnosticContracts())
    return 1;
  for (int i = 1; i < argc; ++i) {
    if (!CheckFixture(argv[i]))
      return 1;
  }
  return 0;
}
