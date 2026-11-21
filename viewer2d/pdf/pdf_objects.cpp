#include "pdf_objects.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

#include <zlib.h>

namespace layout_pdf_internal {

// Creates a formatter with precision constrained to the supported PDF range.
FloatFormatter::FloatFormatter(int precision)
    : precision_(std::clamp(precision, 0, 6)) {}

// Formats a PDF number with fixed precision and locale-independent punctuation.
std::string FloatFormatter::Format(double value) const {
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::fixed << std::setprecision(precision_) << value;
  return ss.str();
}

// Compresses a PDF byte stream using deterministic fast zlib compression.
bool PdfDeflater::Compress(const std::string &input, std::string &output,
                           std::string &error) {
  if (input.empty()) {
    output.clear();
    return true;
  }
  uLongf bound = compressBound(input.size());
  std::string compressed;
  compressed.resize(bound);

  int zres = compress2(reinterpret_cast<Bytef *>(compressed.data()), &bound,
                       reinterpret_cast<const Bytef *>(input.data()),
                       input.size(), Z_BEST_SPEED);
  if (zres != Z_OK) {
    error = "compress2 failed";
    return false;
  }

  compressed.resize(bound);
  output.swap(compressed);
  return true;
}

} // namespace layout_pdf_internal
