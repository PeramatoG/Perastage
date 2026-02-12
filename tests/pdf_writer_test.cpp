#include "pdf_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  const std::filesystem::path outPath =
      std::filesystem::temp_directory_path() / "perastage_pdf_writer_test.pdf";

  std::vector<PdfObject> objects;
  objects.push_back({"<< /Type /Catalog /Pages 2 0 R >>"});
  objects.push_back({"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R >>"});
  objects.push_back({"<< /Length 0 >>\nstream\n\nendstream"});

  std::string error;
  if (!WritePdfDocument(outPath, objects, 1, error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  std::ifstream in(outPath, std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.find("xref") == std::string::npos || data.find("%%EOF") == std::string::npos) {
    std::cerr << "Missing xref or EOF markers" << std::endl;
    return 1;
  }

  std::filesystem::remove(outPath);
  return 0;
}
