#include "pdf_writer.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>

namespace {

constexpr std::streamoff kMaximumClassicXrefOffset = 9999999999LL;

// Reads and validates the current byte offset for a classic PDF xref entry.
bool ReadOffset(std::ofstream &file, std::streamoff &offset,
                const std::string &stage, std::string &error) {
  const std::streampos position = file.tellp();
  if (position == std::streampos(-1)) {
    error = "Unable to determine PDF byte offset after " + stage + ".";
    return false;
  }
  offset = static_cast<std::streamoff>(position);
  if (offset < 0 || offset > kMaximumClassicXrefOffset) {
    error = "PDF byte offset is outside the classic xref range after " +
            stage + ".";
    return false;
  }
  return true;
}

// Verifies that a completed PDF serialization stage left the stream writable.
bool CheckWrite(std::ofstream &file, const std::string &stage,
                std::string &error) {
  if (file.good())
    return true;
  error = "Unable to write PDF " + stage + ".";
  return false;
}

} // namespace

// Serializes PDF objects with checked classic xref offsets and finalization.
bool WritePdfDocument(const std::filesystem::path &outputPath,
                      const std::vector<PdfObject> &objects,
                      size_t catalogObjectIndex, std::string &error) {
  error.clear();
  if (catalogObjectIndex == 0 || catalogObjectIndex > objects.size()) {
    error = "The PDF catalog object index is outside the object table.";
    return false;
  }

  try {
    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      error = "Unable to open the destination file for writing.";
      return false;
    }
    file.imbue(std::locale::classic());

    file << "%PDF-1.4\n";
    if (!CheckWrite(file, "header", error))
      return false;

    std::vector<std::streamoff> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      std::streamoff offset = 0;
      if (!ReadOffset(file, offset, "object " + std::to_string(i + 1), error))
        return false;
      offsets.push_back(offset);
      file << (i + 1) << " 0 obj\n" << objects[i].body << "\nendobj\n";
      if (!CheckWrite(file, "object " + std::to_string(i + 1), error))
        return false;
    }

    std::streamoff xrefPosition = 0;
    if (!ReadOffset(file, xrefPosition, "object table", error))
      return false;
    file << "xref\n0 " << (objects.size() + 1)
         << "\n0000000000 65535 f \n";
    for (const std::streamoff offset : offsets)
      file << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
    if (!CheckWrite(file, "xref table", error))
      return false;

    file << "trailer\n<< /Size " << (objects.size() + 1) << " /Root "
         << catalogObjectIndex << " 0 R >>\n";
    if (!CheckWrite(file, "trailer", error))
      return false;
    file << "startxref\n" << xrefPosition << '\n';
    if (!CheckWrite(file, "startxref", error))
      return false;
    file << "%%EOF";
    if (!CheckWrite(file, "EOF marker", error))
      return false;

    file.flush();
    if (!file.good()) {
      error = "Unable to flush the completed PDF document.";
      return false;
    }
    file.close();
    if (file.fail()) {
      error = "Unable to finalize the completed PDF document.";
      return false;
    }
    return true;
  } catch (const std::exception &exception) {
    error = std::string("Failed to generate PDF content: ") + exception.what();
    return false;
  } catch (...) {
    error = "An unknown error occurred while generating the PDF plan.";
    return false;
  }
}
