#pragma once

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace pdf_test_support {

// Builds the complete ASCII width array used by deterministic test fonts.
inline std::string BuildAsciiWidths() {
  std::ostringstream widths;
  for (int character = 32; character <= 126; ++character) {
    if (character != 32)
      widths << ' ';
    widths << (character == 32 ? 250 : 600);
  }
  return widths.str();
}

// Builds a deterministic, uncompressed PDF containing one content stream per
// page.
inline std::string BuildPdf(const std::vector<std::string> &pageStreams) {
  std::vector<std::string> objects;
  std::ostringstream kids;
  for (size_t i = 0; i < pageStreams.size(); ++i)
    kids << (3 + i * 2) << " 0 R ";
  objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
  objects.push_back("<< /Type /Pages /Kids [" + kids.str() + "] /Count " +
                    std::to_string(pageStreams.size()) + " >>");
  const size_t fontObject = 3 + pageStreams.size() * 2;
  for (size_t i = 0; i < pageStreams.size(); ++i) {
    const size_t contentObject = 4 + i * 2;
    objects.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] "
                      "/Contents " +
                      std::to_string(contentObject) +
                      " 0 R /Resources << /Font << /F1 " +
                      std::to_string(fontObject) + " 0 R >> >> >>");
    objects.push_back("<< /Length " + std::to_string(pageStreams[i].size()) +
                      " >>\nstream\n" + pageStreams[i] + "endstream");
  }
  objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
                    "/Encoding /WinAnsiEncoding /FirstChar 32 /LastChar 126 "
                    "/Widths [" +
                    BuildAsciiWidths() + "] >>");

  std::ostringstream pdf;
  pdf.imbue(std::locale::classic());
  pdf << "%PDF-1.4\n";
  std::vector<std::streamoff> offsets;
  for (size_t i = 0; i < objects.size(); ++i) {
    offsets.push_back(pdf.tellp());
    pdf << (i + 1) << " 0 obj\n" << objects[i] << "\nendobj\n";
  }
  const std::streamoff xref = pdf.tellp();
  pdf << "xref\n0 " << (objects.size() + 1) << "\n0000000000 65535 f \n";
  for (const auto offset : offsets)
    pdf << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
  pdf << "trailer\n<< /Size " << (objects.size() + 1)
      << " /Root 1 0 R >>\nstartxref\n"
      << xref << "\n%%EOF";
  return pdf.str();
}

// Writes exact fixture bytes to a binary file.
inline bool WriteBinaryFile(const std::filesystem::path &path,
                            const std::string &bytes, std::string &error) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    error = "Unable to open fixture for writing: " + path.string();
    return false;
  }
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (output.fail()) {
    error = "Unable to finalize fixture: " + path.string();
    return false;
  }
  return true;
}

// Reads exact bytes from a fixture file.
inline bool ReadBinaryFile(const std::filesystem::path &path,
                           std::string &bytes, std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    error = "Unable to open fixture: " + path.string();
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
  if (input.bad()) {
    error = "Unable to read fixture: " + path.string();
    return false;
  }
  return true;
}

// Validates the deterministic font dictionary used by runtime fixtures.
inline bool ValidateFontDictionary(const std::string &pdf, std::string &error) {
  if (pdf.find("/Type /Font") == std::string::npos ||
      pdf.find("/Subtype /Type1") == std::string::npos ||
      pdf.find("/BaseFont /Helvetica") == std::string::npos ||
      pdf.find("/Encoding /WinAnsiEncoding") == std::string::npos ||
      pdf.find("/FirstChar 32") == std::string::npos ||
      pdf.find("/LastChar 126") == std::string::npos) {
    error = "The deterministic font dictionary is incomplete.";
    return false;
  }
  const size_t widthsStart = pdf.find("/Widths [");
  const size_t valuesStart =
      widthsStart == std::string::npos ? std::string::npos : widthsStart + 9;
  const size_t widthsEnd = pdf.find(']', valuesStart);
  if (valuesStart == std::string::npos || widthsEnd == std::string::npos) {
    error = "The deterministic font width array is missing.";
    return false;
  }
  std::istringstream widths(pdf.substr(valuesStart, widthsEnd - valuesStart));
  std::vector<int> values;
  int width = 0;
  while (widths >> width)
    values.push_back(width);
  if (values.size() != 95 || values.front() <= 0) {
    error =
        "The font must contain exactly 95 widths and an explicit space width.";
    return false;
  }
  return true;
}

// Validates stream lengths, xref metadata, object offsets, and terminal
// markers.
inline bool ValidatePdfStructure(const std::string &pdf, std::string &error) {
  if (pdf.size() < 5) {
    error = "PDF is too short.";
    return false;
  }
  const size_t xref = pdf.find("xref\n");
  const size_t startxref = pdf.rfind("startxref\n");
  if (pdf.rfind("%PDF-", 0) != 0 || xref == std::string::npos ||
      startxref == std::string::npos || pdf.substr(pdf.size() - 5) != "%%EOF") {
    error = "PDF header, xref, startxref, or EOF marker is missing.";
    return false;
  }
  const size_t xrefValueStart = startxref + 10;
  const size_t xrefValueEnd = pdf.find('\n', xrefValueStart);
  if (xrefValueEnd == std::string::npos ||
      std::stoull(pdf.substr(xrefValueStart, xrefValueEnd - xrefValueStart)) !=
          xref) {
    error = "startxref does not point to the xref token.";
    return false;
  }

  std::istringstream lines(pdf.substr(xref + 5, startxref - xref - 5));
  size_t firstObject = 0;
  size_t count = 0;
  if (!(lines >> firstObject >> count) || firstObject != 0 || count < 2) {
    error = "The classic xref header is invalid.";
    return false;
  }
  std::string line;
  std::getline(lines, line);
  for (size_t object = 0; object < count; ++object) {
    if (!std::getline(lines, line) || line.size() < 17) {
      error = "An xref entry is truncated.";
      return false;
    }
    if (object > 0) {
      const size_t offset = std::stoull(line.substr(0, 10));
      const std::string header = std::to_string(object) + " 0 obj";
      if (pdf.compare(offset, header.size(), header) != 0) {
        error = "An xref entry does not point to its object header.";
        return false;
      }
    }
  }
  const std::string trailerSize = "/Size " + std::to_string(count);
  if (pdf.find(trailerSize, xref) == std::string::npos) {
    error = "The xref entry count does not match the trailer size.";
    return false;
  }

  size_t stream = 0;
  while ((stream = pdf.find("stream\n", stream)) != std::string::npos) {
    const size_t dictionary = pdf.rfind("/Length ", stream);
    if (dictionary == std::string::npos) {
      error = "A content stream has no direct length.";
      return false;
    }
    const size_t lengthStart = dictionary + 8;
    const size_t lengthEnd = pdf.find_first_not_of("0123456789", lengthStart);
    const size_t declared =
        std::stoull(pdf.substr(lengthStart, lengthEnd - lengthStart));
    const size_t dataStart = stream + 7;
    const size_t dataEnd = pdf.find("endstream", dataStart);
    if (dataEnd == std::string::npos || dataEnd - dataStart != declared) {
      error = "A content stream length is invalid.";
      return false;
    }
    stream = dataEnd + 9;
  }
  return true;
}

} // namespace pdf_test_support
