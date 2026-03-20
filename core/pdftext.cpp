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

#include <string>

#include <podofo/podofo.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

using namespace PoDoFo;

namespace {

bool HasVisibleText(const std::string &text) {
  for (unsigned char ch : text) {
    if (!std::isspace(ch))
      return true;
  }
  return false;
}

std::string TryExtractWithPodofoTxtextract(const std::string &path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path tempDir = fs::temp_directory_path(ec);
  if (ec)
    return {};

  const auto token =
      std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path outPath = tempDir / ("perastage_rider_extract_" +
                                std::to_string(token) + ".txt");

#if defined(_WIN32)
  const std::string command = "podofotxtextract.exe \"" + path + "\" \"" +
                              outPath.string() + "\"";
#else
  const std::string command =
      "podofotxtextract \"" + path + "\" \"" + outPath.string() + "\"";
#endif
  const int rc = std::system(command.c_str());
  if (rc != 0) {
    fs::remove(outPath, ec);
    return {};
  }

  std::ifstream in(outPath, std::ios::binary);
  if (!in) {
    fs::remove(outPath, ec);
    return {};
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  fs::remove(outPath, ec);
  return ss.str();
}

std::string ExtractAsciiRunsFromPdfBytes(const std::vector<char> &bytes) {
  std::string out;
  std::string run;
  run.reserve(128);

  auto flushRun = [&]() {
    if (run.size() >= 4) {
      if (!out.empty() && out.back() != '\n')
        out.push_back('\n');
      out += run;
    }
    run.clear();
  };

  for (unsigned char c : bytes) {
    const bool printable = (c >= 32 && c <= 126);
    if (printable) {
      run.push_back(static_cast<char>(c));
      continue;
    }
    if (c == '\n' || c == '\r') {
      flushRun();
      continue;
    }
    flushRun();
  }
  flushRun();
  return out;
}

std::string ExtractUtf16AsciiRunsFromPdfBytes(const std::vector<char> &bytes) {
  std::string out;
  auto appendRun = [&](const std::string &run) {
    if (run.size() < 4)
      return;
    if (!out.empty() && out.back() != '\n')
      out.push_back('\n');
    out += run;
  };

  const auto isPrintableAscii = [](unsigned char c) {
    return c >= 32 && c <= 126;
  };

  for (size_t i = 0; i + 1 < bytes.size();) {
    std::string run;
    size_t j = i;

    // UTF-16BE-like ASCII pattern: 00 xx 00 yy ...
    while (j + 1 < bytes.size() && bytes[j] == 0 &&
           isPrintableAscii(static_cast<unsigned char>(bytes[j + 1]))) {
      run.push_back(bytes[j + 1]);
      j += 2;
    }
    appendRun(run);
    if (!run.empty()) {
      i = j;
      continue;
    }

    // UTF-16LE-like ASCII pattern: xx 00 yy 00 ...
    while (j + 1 < bytes.size() &&
           isPrintableAscii(static_cast<unsigned char>(bytes[j])) &&
           bytes[j + 1] == 0) {
      run.push_back(bytes[j]);
      j += 2;
    }
    appendRun(run);
    if (!run.empty()) {
      i = j;
      continue;
    }

    ++i;
  }

  return out;
}

void AppendEntriesToText(const std::vector<PdfTextEntry> &entries,
                         std::string &out) {
  double lastY = std::numeric_limits<double>::quiet_NaN();
  double lastX = 0.0;
  for (const auto &entry : entries) {
    double x = entry.BoundingBox ? entry.BoundingBox->GetLeft() : entry.X;
    double y = entry.BoundingBox ? entry.BoundingBox->GetBottom() : entry.Y;
    double right =
        entry.BoundingBox ? entry.BoundingBox->GetRight() : x + entry.Length;
    if (!std::isnan(lastY)) {
      if (std::fabs(y - lastY) > 2.0) {
        out += '\n';
      } else if (x - lastX > 2.0) {
        out += ' ';
      }
    }
    out += entry.Text;
    lastY = y;
    lastX = right;
  }
}

} // namespace

std::string ExtractPdfText(const std::string &path) {
  try {
    PdfMemDocument doc;
    std::vector<char> pdfBytes;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    std::ifstream pdfFile(path, std::ios::binary);
    if (!pdfFile) {
      std::cerr << "Failed to open PDF file for import: '" << path << "'."
                << std::endl;
      return {};
    }
    pdfFile.seekg(0, std::ios::end);
    const std::streamsize size = pdfFile.tellg();
    if (size <= 0) {
      std::cerr << "PDF file is empty: '" << path << "'." << std::endl;
      return {};
    }
    pdfFile.seekg(0, std::ios::beg);
    pdfBytes.resize(static_cast<size_t>(size));
    if (!pdfFile.read(pdfBytes.data(), size)) {
      std::cerr << "Failed reading PDF bytes for import: '" << path << "'."
                << std::endl;
      return {};
    }
    doc.LoadFromBuffer(bufferview(pdfBytes.data(), pdfBytes.size()));
#else
    doc.Load(path.c_str());
#endif
    std::string out;
    size_t totalEntries = 0;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    auto &pages = doc.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i) {
      auto &page = pages.GetPageAt(i);
      PdfTextExtractParams paramsWithBounds;
      paramsWithBounds.Flags = PdfTextExtractFlags::ComputeBoundingBox;
      std::vector<PdfTextEntry> entries;
      page.ExtractTextTo(entries, paramsWithBounds);
      if (entries.empty()) {
        // Some PDFs do not return entries when bounding boxes are requested.
        // Retry with default extraction params before giving up.
        PdfTextExtractParams defaultParams;
        page.ExtractTextTo(entries, defaultParams);
      }
      totalEntries += entries.size();
      std::sort(entries.begin(), entries.end(), [](const PdfTextEntry &a,
                                                   const PdfTextEntry &b) {
        if (std::fabs(a.Y - b.Y) > 2.0)
          return a.Y > b.Y; // top to bottom
        return a.X < b.X;
      });
      AppendEntriesToText(entries, out);
      out += '\n';
    }
#else
    for (int i = 0; i < doc.GetPageCount(); ++i) {
      PdfPage *page = doc.GetPage(i);
      PdfContentsTokenizer tokenizer(page);
      EPdfContentsType type;
      const char *token = nullptr;
      PdfVariant var;
      std::vector<PdfVariant> stack;
      PdfFont *curFont = nullptr;
      double fontSize = 0.0;
      double curX = 0.0;
      double curY = 0.0;
      double lastX = 0.0;
      bool firstOnLine = true;
      while (tokenizer.ReadNext(type, token, var)) {
        if (type == ePdfContentsType_Variant) {
          stack.push_back(var);
        } else if (type == ePdfContentsType_Keyword) {
          if (!strcmp(token, "BT")) {
            curX = lastX = 0.0;
            firstOnLine = true;
          } else if (!strcmp(token, "ET")) {
            out += '\n';
          } else if (!strcmp(token, "Tf") && stack.size() >= 2) {
            fontSize = stack.back().GetReal();
            stack.pop_back();
            PdfName fontName = stack.back().GetName();
            stack.pop_back();
            PdfObject *fontObj = page->GetFromResources(PdfName("Font"), fontName);
            if (fontObj)
              curFont = doc.GetFont(fontObj);
          } else if ((!strcmp(token, "Td") || !strcmp(token, "TD")) &&
                     stack.size() >= 2) {
            double ty = stack.back().GetReal();
            stack.pop_back();
            double tx = stack.back().GetReal();
            stack.pop_back();
            curX += tx;
            curY += ty;
            if (ty != 0)
              firstOnLine = true;
          } else if ((strcmp(token, "Tj") == 0 || strcmp(token, "'") == 0 ||
                      strcmp(token, "\"") == 0) && !stack.empty() && curFont) {
            PdfString s = stack.back().GetString();
            stack.pop_back();
            if (!firstOnLine && curX - lastX > fontSize * 0.5)
              out += ' ';
            out += s.GetStringUtf8();
            lastX = curX + curFont->GetFontMetrics()->StringWidth(s) * fontSize / 1000.0;
            curX = lastX;
            firstOnLine = false;
            if (strcmp(token, "'") == 0 || strcmp(token, "\"") == 0) {
              out += '\n';
              firstOnLine = true;
            }
          } else if (strcmp(token, "TJ") == 0 && !stack.empty() && curFont) {
            PdfArray arr = stack.back().GetArray();
            stack.pop_back();
            for (size_t j = 0; j < arr.GetSize(); ++j) {
              if (arr[j].IsString()) {
                PdfString s = arr[j].GetString();
                if (!firstOnLine && curX - lastX > fontSize * 0.5)
                  out += ' ';
                out += s.GetStringUtf8();
                lastX = curX + curFont->GetFontMetrics()->StringWidth(s) * fontSize / 1000.0;
                curX = lastX;
                firstOnLine = false;
              } else if (arr[j].IsNumber()) {
                curX += arr[j].GetReal() * fontSize / 1000.0;
              }
            }
          }
        }
      }
      out += '\n';
    }
#endif
    out.erase(std::remove(out.begin(), out.end(), '\0'), out.end());
    if (!out.empty() && out.back() == '\n')
      out.pop_back();
    {
      std::ostringstream oss;
      oss << "PDF import: '" << path << "' -> " << out.size()
          << " chars, " << totalEntries << " text entries.";
      std::cerr << oss.str() << std::endl;
    }
    std::string bestText = out;
    const bool baseVisible = HasVisibleText(out);
    std::cerr << "PDF import base extraction visibility for '" << path
              << "': " << (baseVisible ? "visible" : "not visible")
              << ", chars=" << out.size() << "." << std::endl;

    if (!baseVisible) {
      const std::string toolText = TryExtractWithPodofoTxtextract(path);
      std::cerr << "PDF import podofotxtextract fallback chars for '" << path
                << "': " << toolText.size() << "." << std::endl;
      if (HasVisibleText(toolText)) {
        std::cerr << "PDF import fallback via podofotxtextract succeeded for '"
                  << path << "'." << std::endl;
        return toolText;
      }
      if (bestText.empty() && !toolText.empty())
        bestText = toolText;

      if (!pdfBytes.empty()) {
        const std::string asciiText = ExtractAsciiRunsFromPdfBytes(pdfBytes);
        std::cerr << "PDF import ASCII fallback chars for '" << path
                  << "': " << asciiText.size() << "." << std::endl;
        if (HasVisibleText(asciiText)) {
          std::cerr
              << "PDF import fallback via ASCII run extraction succeeded for '"
              << path << "'." << std::endl;
          return asciiText;
        }
        if (bestText.empty() && !asciiText.empty())
          bestText = asciiText;

        const std::string utf16AsciiText =
            ExtractUtf16AsciiRunsFromPdfBytes(pdfBytes);
        std::cerr << "PDF import UTF16-ASCII fallback chars for '" << path
                  << "': " << utf16AsciiText.size() << "." << std::endl;
        if (HasVisibleText(utf16AsciiText)) {
          std::cerr << "PDF import fallback via UTF16-ASCII run extraction "
                       "succeeded for '"
                    << path << "'." << std::endl;
          return utf16AsciiText;
        }
        if (bestText.empty() && !utf16AsciiText.empty())
          bestText = utf16AsciiText;
      }
      if (bestText.empty()) {
        std::cerr << "PDF import produced no extractable text for '" << path
                  << "'." << std::endl;
        return {};
      }
      std::cerr << "PDF import returning non-visible fallback text for '" << path
                << "' with chars=" << bestText.size() << "." << std::endl;
    }
    return bestText;
  } catch (const PdfError &e) {
    std::cerr << "PoDoFo failed to extract text from '" << path
              << "': " << e.what() << std::endl;
  }
  return {};
}
