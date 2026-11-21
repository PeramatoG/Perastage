#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace layout_pdf_internal {

struct PdfObject {
  std::string body;
};

class FloatFormatter {
public:
  explicit FloatFormatter(int precision);
  std::string Format(double value) const;

private:
  int precision_;
};

class PdfDeflater {
public:
  static bool Compress(const std::string &input, std::string &output,
                       std::string &error);
};

} // namespace layout_pdf_internal
