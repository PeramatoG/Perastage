/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
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

// Requires an explicit extraction diagnostic when a text-show has no font.
bool CheckMissingFontDiagnostic(const std::filesystem::path &directory) {
  const std::string bytes =
      pdf_test_support::BuildPdf({"BT\n72 120 Td\n(No font) Tj\nET\n"});
  std::string error;
  const auto path = directory / "missing-font.pdf";
  if (!pdf_test_support::WriteBinaryFile(path, bytes, error)) {
    std::cerr << error << '\n';
    return false;
  }
  const auto result = ExtractPdfTextWithResult(path.string());
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
  if (result.success ||
      result.error.find("active font metrics") == std::string::npos) {
    std::cerr
        << "Missing-font extraction did not return an explicit diagnostic: "
        << result.error << '\n';
    return false;
  }
#endif
  return true;
}

// Validates and compares one unchanged checked-in compatibility fixture.
bool CheckCheckedInFixture(const std::filesystem::path &path) {
  std::string bytes;
  std::string error;
  if (!pdf_test_support::ReadBinaryFile(path, bytes, error) ||
      !pdf_test_support::ValidatePdfStructure(bytes, error)) {
    std::cerr << "Checked-in fixture structure failed for " << path << ": "
              << error << '\n';
    return false;
  }
  const std::string expected =
      ReadExpected(path.parent_path() / (path.stem().string() + ".txt"));
  const auto result = ExtractPdfTextWithResult(path.string());
  if (!result.success) {
    std::cerr << "Checked-in extraction failed for " << path << ": "
              << result.error << '\n';
    return false;
  }
  if (result.text != expected) {
    std::cerr << "Font dictionary contains /Widths: " << std::boolalpha
              << (bytes.find("/Widths") != std::string::npos) << '\n';
    PrintMismatch(path, expected, result);
    return false;
  }
  return true;
}

// Verifies both unchanged width-less Standard 14 regression fixtures.
bool CheckCheckedInRegressions(const std::filesystem::path &directory) {
  return CheckCheckedInFixture(directory / "simple.pdf") &&
         CheckCheckedInFixture(directory / "translated.pdf");
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

// Runs runtime controls before unchanged checked-in compatibility regressions.
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Expected the checked-in PDF fixture directory.\n";
    return 1;
  }
  TemporaryDirectory directory;
  if (!directory.valid()) {
    std::cerr << "Unable to create PDF text temporary directory: "
              << directory.error() << '\n';
    return 1;
  }
  const std::string simple = "BT\n/F1 24 Tf\n72 120 Td\n(Hello World) Tj\nET\n";
  const std::string translated =
      "BT\n/F1 24 Tf\n72 120 Td\n(Foo) Tj\n50 0 Td\n(Bar) Tj\nET\n";
  const std::string adjacent =
      "BT\n/F1 24 Tf\n72 120 Td\n(Foo) Tj\n(Bar) Tj\nET\n";
  const std::string tjGap =
      "BT\n/F1 24 Tf\n72 120 Td\n[(Foo) -1000 (Bar)] TJ\nET\n";
  const std::string tjAdjacent =
      "BT\n/F1 24 Tf\n72 120 Td\n[(Foo) 0 (Bar)] TJ\nET\n";
  const std::string twoLines = "BT\n/F1 24 Tf\n72 150 Td\n(One) Tj\nET\n"
                               "BT\n/F1 24 Tf\n72 100 Td\n(Two) Tj\nET\n";
  const std::string rotated =
      "BT\n/F1 24 Tf\n0 1 -1 0 72 120 Tm\n(Rotated) Tj\nET\n";
  const bool runtimePassed =
      CheckExpectedCanonicalization(directory.path()) &&
      CheckReconstruction() && CheckDiagnosticContracts(directory.path()) &&
      CheckMissingFontDiagnostic(directory.path()) &&
      CheckRuntimeFixture(directory.path(), "simple", simple, "Hello World") &&
      CheckRuntimeFixture(directory.path(), "translated", translated,
                          "Foo Bar") &&
      CheckRuntimeFixture(directory.path(), "adjacent", adjacent, "FooBar") &&
      CheckRuntimeFixture(directory.path(), "literal-space", simple,
                          "Hello World") &&
      CheckRuntimeFixture(directory.path(), "tj-gap", tjGap, "Foo Bar") &&
      CheckRuntimeFixture(directory.path(), "tj-adjacent", tjAdjacent,
                          "FooBar") &&
      CheckRuntimeFixture(directory.path(), "two-lines", twoLines,
                          "One\nTwo") &&
      CheckRuntimeFixture(directory.path(), "rotated", rotated, "Rotated");
  return runtimePassed &&
                 CheckCheckedInRegressions(std::filesystem::path(argv[1]))
             ? 0
             : 1;
}
