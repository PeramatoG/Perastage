#include "pdf_draw_commands.h"
#include "pdf_objects.h"
#include "pdf_writer.h"
#include "support/pdf_test_fixture_builder.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>

namespace {

class CommaDecimalPunctuation : public std::numpunct<char> {
protected:
  // Supplies a comma decimal point without requiring an installed OS locale.
  char do_decimal_point() const override { return ','; }
};

class TemporaryDirectory {
public:
  // Creates a unique test-owned temporary directory.
  TemporaryDirectory() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("perastage_pdf_writer_" + std::to_string(unique));
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    error.clear();
    std::filesystem::create_directories(path_, error);
    if (error)
      creationError_ = error.message();
  }

  // Removes all test output after callers have closed their streams.
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    if (error)
      std::cerr << "Unable to clean PDF writer temporary directory: "
                << error.message() << '\n';
  }

  // Returns the temporary directory path.
  const std::filesystem::path &Path() const { return path_; }

  // Returns a diagnostic when temporary directory creation failed.
  const std::string &CreationError() const { return creationError_; }

private:
  std::filesystem::path path_;
  std::string creationError_;
};

// Verifies locale-independent float and draw-command serialization.
bool CheckLocaleIndependentNumbers() {
  const std::locale original = std::locale();
  std::locale::global(std::locale(original, new CommaDecimalPunctuation));
  struct LocaleRestore {
    std::locale original;
    ~LocaleRestore() { std::locale::global(original); }
  } restore{original};

  layout_pdf_internal::FloatFormatter formatter(3);
  if (formatter.Format(1.5) != "1.500" ||
      formatter.Format(-0.25) != "-0.250") {
    std::cerr << "FloatFormatter used locale-dependent decimal punctuation.\n";
    return false;
  }

  layout_pdf_internal::Mapping mapping;
  mapping.scale = 1.0;
  mapping.flipY = false;
  layout_pdf_internal::Transform transform;
  layout_pdf_internal::RenderOptions options;
  layout_pdf_internal::GraphicsStateCache cache;
  LineCommand line;
  line.x0 = 0.0f;
  line.y0 = 0.0f;
  line.x1 = 10.0f;
  line.y1 = 10.0f;
  line.stroke.width = 1.0f;
  line.stroke.color = {0.0f, 0.0f, 0.0f};

  std::ostringstream content;
  layout_pdf_internal::EmitCommandStroke(content, cache, formatter, mapping,
                                         transform, CanvasCommand{line}, options);
  const std::string expected =
      "1 j\n1 J\n0.000 0.000 0.000 RG\n1.000 w\n"
      "0.000 0.000 m\n10.000 10.000 l\nS\n";
  if (content.str() != expected || content.str().find(',') != std::string::npos) {
    std::cerr << "Draw command output changed or contains a comma decimal separator.\n"
              << content.str();
    return false;
  }
  return true;
}

// Verifies PDF structure, root selection, stream lifecycle, and cleanup.
bool CheckWriterSerialization() {
  TemporaryDirectory temporary;
  if (!temporary.CreationError().empty()) {
    std::cerr << "Unable to create PDF writer temporary directory: "
              << temporary.CreationError() << '\n';
    return false;
  }
  const auto outputPath = temporary.Path() / "document.pdf";
  std::error_code errorCode;
  std::filesystem::remove(outputPath, errorCode);
  if (errorCode) {
    std::cerr << "Unable to remove stale PDF output: " << errorCode.message() << '\n';
    return false;
  }

  const std::vector<PdfObject> objects = {
      {"<< /Type /Catalog /Pages 2 0 R >>"},
      {"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"},
      {"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R >>"},
      {"<< /Length 0 >>\nstream\nendstream"}};
  std::string error;
  if (!WritePdfDocument(outputPath, objects, 1, error)) {
    std::cerr << "PDF writer stage failed: " << error << '\n';
    return false;
  }
  std::string data;
  {
    std::ifstream input(outputPath, std::ios::binary);
    if (!input.is_open()) {
      std::cerr << "Unable to open serialized PDF for validation.\n";
      return false;
    }
    data.assign(std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
    if (input.bad()) {
      std::cerr << "Unable to read serialized PDF for validation.\n";
      return false;
    }
  }

  if (!pdf_test_support::ValidatePdfStructure(data, error) ||
      data.find("trailer\n<< /Size 5 /Root 1 0 R >>") == std::string::npos) {
    std::cerr << "Serialized PDF structure failed validation: " << error << '\n';
    return false;
  }
  error.clear();
  if (WritePdfDocument(outputPath, objects, 0, error) || error.empty()) {
    std::cerr << "Invalid catalog object index was not diagnosed.\n";
    return false;
  }

  std::filesystem::remove(outputPath, errorCode);
  if (errorCode || std::filesystem::exists(outputPath)) {
    std::cerr << "Unable to remove closed PDF output: " << errorCode.message() << '\n';
    return false;
  }
  return true;
}

} // namespace

// Runs PDF writer portability and serialization controls.
int main() {
  return CheckWriterSerialization() && CheckLocaleIndependentNumbers() ? 0 : 1;
}
