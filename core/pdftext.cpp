/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "pdftext.h"

#include <podofo/podofo.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <cstring>

using namespace PoDoFo;

namespace {

constexpr double kFallbackCoordinateTolerance = 2.0;

// Returns the fragment coordinate used to order text horizontally.
double FragmentLeft(const PdfTextFragment &fragment) {
  return fragment.hasBoundingBox ? fragment.left : fragment.x;
}

// Returns the best available end position for measuring an inter-fragment gap.
double FragmentRight(const PdfTextFragment &fragment) {
  const double start = FragmentLeft(fragment);
  const double advanced = fragment.x + std::max(0.0, fragment.advance);
  if (fragment.hasBoundingBox && fragment.right > start)
    return std::max(fragment.right, advanced);
  return std::max(start, advanced);
}

// Derives a coordinate tolerance from fragment height with a conservative fallback.
double GeometryTolerance(const PdfTextFragment &fragment) {
  if (fragment.hasBoundingBox) {
    const double height = std::fabs(fragment.top - fragment.bottom);
    if (height > 0.0)
      return std::max(0.5, height * 0.2);
  }
  return kFallbackCoordinateTolerance;
}

// Removes embedded NUL bytes required by the historical string API contract.
void RemoveEmbeddedNul(std::string &text) {
  text.erase(std::remove(text.begin(), text.end(), '\0'), text.end());
}

#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
// Converts a PoDoFo entry into the dependency-neutral reconstruction model.
PdfTextFragment ConvertEntry(const PdfTextEntry &entry) {
  PdfTextFragment fragment;
  fragment.text = entry.Text;
  fragment.x = entry.X;
  fragment.y = entry.Y;
  fragment.advance = entry.Length;
  if (entry.BoundingBox) {
    fragment.hasBoundingBox = true;
    fragment.left = entry.BoundingBox->GetLeft();
    fragment.right = entry.BoundingBox->GetRight();
    fragment.bottom = entry.BoundingBox->GetBottom();
    fragment.top = entry.BoundingBox->GetTop();
  }
  return fragment;
}
#endif

// Classifies a PoDoFo failure without exposing dependency types through the API.
std::string PdfErrorMessage(const PdfError &error) {
  const std::string detail = error.what();
  if (detail.find("Unsupported") != std::string::npos)
    return "Unsupported PDF content: " + detail;
  return "Malformed or unreadable PDF: " + detail;
}

} // namespace

// Reconstructs deterministic lines and spaces from positioned text fragments.
std::string ReconstructPdfText(std::vector<PdfTextFragment> fragments) {
  std::stable_sort(fragments.begin(), fragments.end(),
                   [](const PdfTextFragment &a, const PdfTextFragment &b) {
                     return a.y > b.y;
                   });
  for (auto lineBegin = fragments.begin(); lineBegin != fragments.end();) {
    auto lineEnd = lineBegin + 1;
    while (lineEnd != fragments.end() &&
           std::fabs(lineEnd->y - lineBegin->y) <=
               std::max(GeometryTolerance(*lineBegin),
                        GeometryTolerance(*lineEnd))) {
      ++lineEnd;
    }
    std::stable_sort(lineBegin, lineEnd,
                     [](const PdfTextFragment &a, const PdfTextFragment &b) {
                       return FragmentLeft(a) < FragmentLeft(b);
                     });
    lineBegin = lineEnd;
  }

  std::string text;
  const PdfTextFragment *previous = nullptr;
  for (const auto &fragment : fragments) {
    if (previous != nullptr) {
      const double tolerance = std::max(GeometryTolerance(*previous),
                                        GeometryTolerance(fragment));
      if (std::fabs(fragment.y - previous->y) > tolerance) {
        text += '\n';
      } else {
        const double gap = FragmentLeft(fragment) - FragmentRight(*previous);
        const bool alreadySeparated =
            (!text.empty() && text.back() == ' ') ||
            (!fragment.text.empty() && fragment.text.front() == ' ');
        if (gap > tolerance && !alreadySeparated)
          text += ' ';
      }
    }
    text += fragment.text;
    previous = &fragment;
  }
  RemoveEmbeddedNul(text);
  return text;
}

// Extracts PDF text and returns parser status separately from extracted content.
PdfTextExtractionResult ExtractPdfTextWithResult(const std::string &path) {
  PdfTextExtractionResult result;
  std::error_code filesystemError;
  if (!std::filesystem::is_regular_file(path, filesystemError)) {
    result.error = "Unable to read PDF file: " + path;
    return result;
  }

  try {
    PdfMemDocument document;
    document.Load(path.c_str());
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    auto &pages = document.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i) {
      auto &page = pages.GetPageAt(i);
      PdfTextExtractParams params;
      params.Flags = PdfTextExtractFlags::ComputeBoundingBox;
      std::vector<PdfTextEntry> entries;
      page.ExtractTextTo(entries, std::string_view{}, params);
      std::vector<PdfTextFragment> fragments;
      fragments.reserve(entries.size());
      std::transform(entries.begin(), entries.end(),
                     std::back_inserter(fragments), ConvertEntry);
      result.pages.push_back(fragments);
      const std::string pageText = ReconstructPdfText(std::move(fragments));
      if (i != 0 && (!result.text.empty() || !pageText.empty()))
        result.text += '\n';
      result.text += pageText;
    }
#else
    for (int i = 0; i < document.GetPageCount(); ++i) {
      PdfPage *page = document.GetPage(i);
      PdfContentsTokenizer tokenizer(page);
      EPdfContentsType type;
      const char *token = nullptr;
      PdfVariant value;
      std::vector<PdfVariant> operands;
      PdfFont *font = nullptr;
      double fontSize = 0.0;
      double currentX = 0.0;
      double lastRight = 0.0;
      bool firstOnLine = true;
      while (tokenizer.ReadNext(type, token, value)) {
        if (type == ePdfContentsType_Variant) {
          operands.push_back(value);
        } else if (type == ePdfContentsType_Keyword) {
          if (std::strcmp(token, "BT") == 0) {
            currentX = lastRight = 0.0;
            firstOnLine = true;
          } else if (std::strcmp(token, "ET") == 0) {
            if (!result.text.empty() && result.text.back() != '\n')
              result.text += '\n';
          } else if (std::strcmp(token, "Tf") == 0 && operands.size() >= 2) {
            fontSize = operands.back().GetReal();
            operands.pop_back();
            const PdfName fontName = operands.back().GetName();
            operands.pop_back();
            PdfObject *fontObject =
                page->GetFromResources(PdfName("Font"), fontName);
            if (fontObject)
              font = document.GetFont(fontObject);
          } else if ((std::strcmp(token, "Td") == 0 ||
                      std::strcmp(token, "TD") == 0) &&
                     operands.size() >= 2) {
            const double y = operands.back().GetReal();
            operands.pop_back();
            currentX += operands.back().GetReal();
            operands.pop_back();
            if (y != 0.0)
              firstOnLine = true;
          } else if ((std::strcmp(token, "Tj") == 0 ||
                      std::strcmp(token, "'") == 0 ||
                      std::strcmp(token, "\"") == 0) &&
                     !operands.empty() && font) {
            const PdfString string = operands.back().GetString();
            operands.pop_back();
            if (!firstOnLine && currentX - lastRight > fontSize * 0.2)
              result.text += ' ';
            result.text += string.GetStringUtf8();
            lastRight = currentX +
                font->GetFontMetrics()->StringWidth(string) * fontSize / 1000.0;
            currentX = lastRight;
            firstOnLine = false;
            if (std::strcmp(token, "'") == 0 || std::strcmp(token, "\"") == 0) {
              result.text += '\n';
              firstOnLine = true;
            }
          } else if (std::strcmp(token, "TJ") == 0 && !operands.empty() && font) {
            const PdfArray array = operands.back().GetArray();
            operands.pop_back();
            for (size_t j = 0; j < array.GetSize(); ++j) {
              if (array[j].IsString()) {
                const PdfString string = array[j].GetString();
                if (!firstOnLine && currentX - lastRight > fontSize * 0.2)
                  result.text += ' ';
                result.text += string.GetStringUtf8();
                lastRight = currentX + font->GetFontMetrics()->StringWidth(string) *
                                           fontSize / 1000.0;
                currentX = lastRight;
                firstOnLine = false;
              } else if (array[j].IsNumber()) {
                currentX += array[j].GetReal() * fontSize / 1000.0;
              }
            }
          }
        }
      }
    }
    while (!result.text.empty() && result.text.back() == '\n')
      result.text.pop_back();
    RemoveEmbeddedNul(result.text);
#endif
    result.success = true;
    return result;
  } catch (const PdfError &error) {
    result.error = PdfErrorMessage(error);
  } catch (const std::exception &error) {
    result.error = std::string("Unable to extract PDF text: ") + error.what();
  }
  return result;
}

// Preserves the legacy empty-string-on-failure extraction interface.
std::string ExtractPdfText(const std::string &path) {
  return ExtractPdfTextWithResult(path).text;
}
