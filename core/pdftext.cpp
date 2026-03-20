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
#include <cmath>
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
  try {
    PdfMemDocument doc;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    doc.Load(path.c_str());
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
      std::cerr << "PDF import page " << (i + 1) << "/" << pages.GetCount()
                << " entries=" << entries.size()
                << " nonEmptyEntries=" << nonEmptyEntries
                << " entryChars=" << entryChars
                << " pageTextChars=" << pageText.size()
                << " visible=" << (HasVisibleText(pageText) ? "yes" : "no")
                << " preview=\"" << BuildPreview(pageText) << "\""
                << std::endl;
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
    if (!HasVisibleText(out)) {
      std::cerr << "PDF import produced no extractable text for '" << path
                << "'." << std::endl;
      return {};
    }
    return out;
  } catch (const PdfError &e) {
    std::cerr << "PoDoFo failed to extract text from '" << path
              << "': " << e.what() << std::endl;
  }
  return {};
}
