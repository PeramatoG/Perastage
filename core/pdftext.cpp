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
#include "logger.h"

#include <string>

#include <podofo/podofo.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <vector>

using namespace PoDoFo;

namespace {
std::mutex g_pdfReportMutex;
std::string g_pdfReport;

void ResetPdfReport(const std::string &path) {
  std::lock_guard<std::mutex> lock(g_pdfReportMutex);
  g_pdfReport = "PDF extraction report for '" + path + "'";
}

void AppendPdfReportLine(const std::string &line) {
  std::lock_guard<std::mutex> lock(g_pdfReportMutex);
  if (!g_pdfReport.empty())
    g_pdfReport += "\n";
  g_pdfReport += line;
}

void LogPdfInfo(const std::string &line) {
  Logger::Instance().Log(Logger::Level::Info, line);
  AppendPdfReportLine(line);
}

void LogPdfWarn(const std::string &line) {
  Logger::Instance().Log(Logger::Level::Warn, line);
  AppendPdfReportLine(line);
}

void LogPdfError(const std::string &line) {
  Logger::Instance().Log(Logger::Level::Error, line);
  AppendPdfReportLine(line);
}


bool HasVisibleText(const std::string &text) {
  for (unsigned char ch : text) {
    if (!std::isspace(ch))
      return true;
  }
  return false;
}

std::string BuildPreview(const std::string &text, size_t maxLen = 120) {
  std::string preview = text.substr(0, std::min(text.size(), maxLen));
  for (char &ch : preview) {
    if (ch == '\n' || ch == '\r' || ch == '\t')
      ch = ' ';
  }
  return preview;
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
  ResetPdfReport(path);
  try {
    PdfMemDocument doc;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    std::ifstream pdfFile(std::filesystem::u8path(path), std::ios::binary);
    if (!pdfFile) {
      LogPdfError("PoDoFo input file could not be opened: '" + path + "'.");
      return {};
    }
    pdfFile.seekg(0, std::ios::end);
    const std::streamsize size = pdfFile.tellg();
    if (size <= 0) {
      LogPdfWarn("PoDoFo input file is empty: '" + path + "'.");
      return {};
    }
    pdfFile.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<size_t>(size));
    if (!pdfFile.read(bytes.data(), size)) {
      LogPdfError("PoDoFo input file read failed: '" + path + "'.");
      return {};
    }
    LogPdfInfo("PoDoFo input bytes loaded: " + std::to_string(bytes.size()) +
               " bytes from '" + path + "'.");
    doc.LoadFromBuffer(bufferview(bytes.data(), bytes.size()));
#else
    doc.Load(path.c_str());
#endif
    std::string out;
    size_t totalEntries = 0;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    auto &pages = doc.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i) {
      auto &page = pages.GetPageAt(i);
      PdfTextExtractParams params;
      std::vector<PdfTextEntry> entries;
      page.ExtractTextTo(entries, std::string_view{}, params);
      totalEntries += entries.size();
      size_t nonEmptyEntries = 0;
      size_t entryChars = 0;
      for (const auto &entry : entries) {
        entryChars += entry.Text.size();
        if (!entry.Text.empty())
          ++nonEmptyEntries;
      }
      std::sort(entries.begin(), entries.end(), [](const PdfTextEntry &a,
                                                   const PdfTextEntry &b) {
        if (std::fabs(a.Y - b.Y) > 2.0)
          return a.Y > b.Y; // top to bottom
        return a.X < b.X;
      });
      std::string pageText;
      AppendEntriesToText(entries, pageText);
      if (!pageText.empty())
        out += pageText + '\n';
      std::ostringstream pageLog;
      pageLog << "PDF import page " << (i + 1) << "/" << pages.GetCount()
              << " entries=" << entries.size()
              << " nonEmptyEntries=" << nonEmptyEntries
              << " entryChars=" << entryChars
              << " pageTextChars=" << pageText.size()
              << " visible=" << (HasVisibleText(pageText) ? "yes" : "no")
              << " preview=\"" << BuildPreview(pageText) << "\"";
      LogPdfInfo(pageLog.str());
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
      LogPdfInfo(oss.str());
    }
    if (!HasVisibleText(out)) {
      LogPdfWarn("PDF import produced no extractable text for '" + path + "'.");
      return {};
    }
    return out;
  } catch (const PdfError &e) {
    LogPdfError("PoDoFo failed to extract text from '" + path +
                "': " + std::string(e.what()));
  }
  return {};
}

std::string GetLastPdfTextExtractionReport() {
  std::lock_guard<std::mutex> lock(g_pdfReportMutex);
  return g_pdfReport;
}
