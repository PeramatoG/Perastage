#include "pdftext.h"
#include "support/pdf_test_fixture_builder.h"

#include <podofo/podofo.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

// Owns a unique temporary directory and removes it on every exit path.
class TemporaryDirectory {
public:
  // Creates a uniquely named directory under the system temporary directory.
  TemporaryDirectory() {
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("perastage_pdf_text_" + std::to_string(unique));
    std::error_code error;
    std::filesystem::create_directories(path_, error);
    if (error)
      error_ = error.message();
  }

  // Removes all test-owned files without throwing during teardown.
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  // Returns the directory used by this test invocation.
  const std::filesystem::path &path() const { return path_; }

  // Reports whether directory creation succeeded.
  bool valid() const { return error_.empty(); }

  // Returns the filesystem error reported during construction.
  const std::string &error() const { return error_; }

private:
  std::filesystem::path path_;
  std::string error_;
};

// Canonicalizes platform line endings and removes only the historical suffix.
std::string CanonicalizeExpected(std::string text) {
  std::string normalized;
  normalized.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r') {
      if (i + 1 < text.size() && text[i + 1] == '\n')
        ++i;
      normalized += '\n';
    } else {
      normalized += text[i];
    }
  }
  while (!normalized.empty() &&
         (normalized.back() == '\n' || normalized.back() == '\f'))
    normalized.pop_back();
  return normalized;
}

// Reads expected text as bytes before applying narrow line-ending
// canonicalization.
std::string ReadExpected(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return CanonicalizeExpected(std::string(std::istreambuf_iterator<char>(file),
                                          std::istreambuf_iterator<char>()));
}

// Escapes bytes so whitespace and control characters remain visible in
// diagnostics.
std::string EscapeText(const std::string &text) {
  std::ostringstream escaped;
  for (const unsigned char byte : text) {
    switch (byte) {
    case '\\':
      escaped << "\\\\";
      break;
    case '\n':
      escaped << "\\n";
      break;
    case '\r':
      escaped << "\\r";
      break;
    case '\t':
      escaped << "\\t";
      break;
    case '\f':
      escaped << "\\f";
      break;
    default:
      if (byte >= 32 && byte <= 126)
        escaped << static_cast<char>(byte);
      else
        escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(byte) << std::dec;
    }
  }
  return escaped.str() + "<EOS>";
}

// Formats one byte or an explicit end-of-string marker.
std::string FormatByte(const std::string &text, size_t index) {
  if (index >= text.size())
    return "<EOS>";
  std::ostringstream value;
  value << "0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(static_cast<unsigned char>(text[index]));
  return value.str();
}

// Prints strict byte-comparison and raw positioned-entry diagnostics.
void PrintMismatch(const std::filesystem::path &path,
                   const std::string &expected,
                   const PdfTextExtractionResult &result) {
  size_t mismatch = 0;
  while (mismatch < expected.size() && mismatch < result.text.size() &&
         expected[mismatch] == result.text[mismatch])
    ++mismatch;
  std::cerr << "Mismatch for " << path
            << "\nExpected byte length: " << expected.size()
            << "\nActual byte length: " << result.text.size()
            << "\nEscaped expected: " << EscapeText(expected)
            << "\nEscaped actual: " << EscapeText(result.text)
            << "\nFirst mismatch index: " << mismatch
            << "\nExpected byte: " << FormatByte(expected, mismatch)
            << "\nActual byte: " << FormatByte(result.text, mismatch)
            << "\nPODOFO_VERSION: " << PODOFO_VERSION << '\n';
  for (size_t page = 0; page < result.pages.size(); ++page) {
    std::cerr << "Page index: " << page
              << "\nEntry count: " << result.pages[page].size() << '\n';
    for (const auto &entry : result.pages[page]) {
      std::cerr << "  Text: " << EscapeText(entry.text) << " X: " << entry.x
                << " Y: " << entry.y << " Length: " << entry.advance
                << " Bounding box present: " << std::boolalpha
                << entry.hasBoundingBox << " left: " << entry.left
                << " bottom: " << entry.bottom << " right: " << entry.right
                << " top: " << entry.top << '\n';
    }
  }
}

// Builds, validates, writes, extracts, and strictly compares one runtime
// fixture.
bool CheckRuntimeFixture(const std::filesystem::path &directory,
                         const std::string &name, const std::string &stream,
                         const std::string &expected) {
  const std::string bytes = pdf_test_support::BuildPdf({stream});
  std::string error;
  if (!pdf_test_support::ValidatePdfStructure(bytes, error) ||
      !pdf_test_support::ValidateFontDictionary(bytes, error)) {
    std::cerr << "Fixture structure failed for " << name << ": " << error
              << '\n';
    return false;
  }
  const auto path = directory / (name + ".pdf");
  if (!pdf_test_support::WriteBinaryFile(path, bytes, error)) {
    std::cerr << error << '\n';
    return false;
  }
  const auto result = ExtractPdfTextWithResult(path.string());
  if (!result.success) {
    std::cerr << "Extraction failed for " << path << ": " << result.error
              << '\n';
    return false;
  }
  if (result.text != expected) {
    PrintMismatch(path, expected, result);
    return false;
  }
  return true;
}

// Verifies CRLF, lone-CR, spaces, blank lines, and historical suffix handling.
bool CheckExpectedCanonicalization(const std::filesystem::path &directory) {
  const auto path = directory / "expected.txt";
  std::string error;
  if (!pdf_test_support::WriteBinaryFile(
          path, "Alpha  Beta\r\n\rGamma\r\n\r\n\f", error)) {
    std::cerr << error << '\n';
    return false;
  }
  const std::string expected = "Alpha  Beta\n\nGamma";
  const std::string actual = ReadExpected(path);
  if (actual != expected) {
    std::cerr << "Expected-text canonicalization mismatch: "
              << EscapeText(actual) << '\n';
    return false;
  }
  return true;
}

// Verifies positioned reconstruction without depending on PDF parser behavior.
bool CheckReconstruction() {
  auto fragment = [](std::string text, double x, double y, double advance) {
    return PdfTextFragment{std::move(text), x, y, advance};
  };
  const std::vector<std::pair<std::vector<PdfTextFragment>, std::string>>
      cases = {{{fragment("Hello World", 0, 20, 60)}, "Hello World"},
               {{fragment("Foo", 0, 20, 18), fragment("Bar", 68, 20, 18)},
                "Foo Bar"},
               {{fragment("Foo", 0, 20, 18), fragment("Bar", 18.5, 20, 18)},
                "FooBar"},
               {{fragment("Foo ", 0, 20, 18), fragment("Bar", 68, 20, 18)},
                "Foo Bar"},
               {{fragment("Second", 0, 10, 30), fragment("First", 0, 20, 25)},
                "First\nSecond"}};
  for (const auto &[fragments, expected] : cases) {
    if (const std::string actual = ReconstructPdfText(fragments);
        actual != expected) {
      std::cerr << "Reconstruction mismatch: expected '" << expected
                << "', actual '" << actual << "'\n";
      return false;
    }
  }
  return true;
}

// Verifies empty, malformed, missing, and multi-page diagnostic contracts.
bool CheckDiagnosticContracts(const std::filesystem::path &directory) {
  std::string error;
  const auto emptyPath = directory / "empty.pdf";
  const auto pagesPath = directory / "pages.pdf";
  const auto malformedPath = directory / "malformed.pdf";
  if (!pdf_test_support::WriteBinaryFile(
          emptyPath, pdf_test_support::BuildPdf({""}), error) ||
      !pdf_test_support::WriteBinaryFile(
          pagesPath,
          pdf_test_support::BuildPdf(
              {"BT\n/F1 12 Tf\n10 20 Td\n(One) Tj\nET\n",
               "BT\n/F1 12 Tf\n10 20 Td\n(Two) Tj\nET\n"}),
          error) ||
      !pdf_test_support::WriteBinaryFile(malformedPath, "%PDF-1.4\ninvalid",
                                         error)) {
    std::cerr << error << '\n';
    return false;
  }
  const auto empty = ExtractPdfTextWithResult(emptyPath.string());
  const auto pages = ExtractPdfTextWithResult(pagesPath.string());
  const auto malformed = ExtractPdfTextWithResult(malformedPath.string());
  const auto missing =
      ExtractPdfTextWithResult((directory / "missing.pdf").string());
  if (!empty.success || !empty.text.empty() || !pages.success ||
      pages.text != "One\nTwo" || malformed.success ||
      malformed.error.empty() || missing.success ||
      missing.error.find("Unable to read") == std::string::npos) {
    std::cerr << "PDF extraction result contract failed. Empty error: "
              << empty.error << "; pages: " << pages.text
              << "; malformed: " << malformed.error
              << "; missing: " << missing.error << '\n';
    return false;
  }
  return true;
}

} // namespace

// Runs runtime fixture, reconstruction, normalization, and diagnostic controls.
int main() {
  TemporaryDirectory directory;
  if (!directory.valid()) {
    std::cerr << "Unable to create PDF text temporary directory: "
              << directory.error() << '\n';
    return 1;
  }
  const std::string simple = "BT\n/F1 24 Tf\n72 120 Td\n(Hello World) Tj\nET\n";
  const std::string translated =
      "BT\n/F1 24 Tf\n72 120 Td\n(Foo) Tj\n50 0 Td\n(Bar) Tj\nET\n";
  return CheckExpectedCanonicalization(directory.path()) &&
                 CheckReconstruction() &&
                 CheckDiagnosticContracts(directory.path()) &&
                 CheckRuntimeFixture(directory.path(), "simple", simple,
                                     "Hello World") &&
                 CheckRuntimeFixture(directory.path(), "translated", translated,
                                     "Foo Bar")
             ? 0
             : 1;
}
