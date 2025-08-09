#include "pdftext.h"

#include <string>
#include <wx/log.h>

#include <podofo/podofo.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>

using namespace PoDoFo;

std::string ExtractPdfText(const std::string &path) {
  try {
    PdfMemDocument doc;
    doc.Load(path.c_str());
    std::string out;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    auto &pages = doc.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i) {
      auto &page = pages.GetPageAt(i);
      PdfTextExtractParams params;
      params.Flags = PdfTextExtractFlags::ComputeBoundingBox;
      std::vector<PdfTextEntry> entries;
      page.ExtractTextTo(entries, params);
      std::sort(entries.begin(), entries.end(),
                [](const PdfTextEntry &a, const PdfTextEntry &b) {
                  if (std::fabs(a.Y - b.Y) > 2.0)
                    return a.Y > b.Y; // top to bottom
                  return a.X < b.X;
                });
      double lastY = std::numeric_limits<double>::quiet_NaN();
      double lastX = 0.0;
      for (const auto &e : entries) {
        double x = e.BoundingBox ? e.BoundingBox->GetLeft() : e.X;
        double y = e.BoundingBox ? e.BoundingBox->GetBottom() : e.Y;
        double right = e.BoundingBox ? e.BoundingBox->GetRight() : x + e.Length;
        if (!std::isnan(lastY)) {
          if (std::fabs(y - lastY) > 2.0) {
            out += '\n';
          } else if (x - lastX > 2.0) {
            out += ' ';
          }
        }
        out += e.Text;
        lastY = y;
        lastX = right;
      }
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
    if (!out.empty() && out.back() == '\n')
      out.pop_back();
    return out;
  } catch (const PdfError &e) {
    wxLogError("PoDoFo failed to extract text from '%s': %s", path.c_str(),
               e.what());
  }
  return {};
}
