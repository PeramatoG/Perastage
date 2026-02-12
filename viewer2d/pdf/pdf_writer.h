#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct PdfObject {
  std::string body;
};

bool WritePdfDocument(const std::filesystem::path &outputPath,
                      const std::vector<PdfObject> &objects,
                      size_t catalogObjectIndex, std::string &error);
