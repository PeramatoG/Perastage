#include "pdf_writer.h"

#include <fstream>
#include <iomanip>

bool WritePdfDocument(const std::filesystem::path &outputPath,
                      const std::vector<PdfObject> &objects,
                      size_t catalogObjectIndex, std::string &error) {
  try {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
      error = "Unable to open the destination file for writing.";
      return false;
    }

    file << "%PDF-1.4\n";
    std::vector<long> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets.push_back(static_cast<long>(file.tellp()));
      file << (i + 1) << " 0 obj\n" << objects[i].body << "\nendobj\n";
    }

    long xrefPos = static_cast<long>(file.tellp());
    file << "xref\n0 " << (objects.size() + 1) << "\n0000000000 65535 f \n";
    for (long off : offsets)
      file << std::setw(10) << std::setfill('0') << off << " 00000 n \n";

    file << "trailer\n<< /Size " << (objects.size() + 1) << " /Root "
         << catalogObjectIndex << " 0 R >>\nstartxref\n"
         << xrefPos << "\n%%EOF";
    return true;
  } catch (const std::exception &ex) {
    error = std::string("Failed to generate PDF content: ") + ex.what();
    return false;
  } catch (...) {
    error = "An unknown error occurred while generating the PDF plan.";
    return false;
  }
}
