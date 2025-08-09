#include "pdftext.h"

#include <cstdio>
#include <string>
#include <wx/log.h>

#ifdef _WIN32
#define NOMINMAX
#define popen _popen
#define pclose _pclose
#include <windows.h>
#endif

#include <podofo/podofo.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace PoDoFo;

std::string ExtractPdfText(const std::string &path) {
  // Try using the external "pdftotext" command first for better layout
  {
    std::string cmd = "pdftotext -layout \"" + path + "\" -";
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = NULL, writePipe = NULL;
    if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
      STARTUPINFOA si{};
      si.cb = sizeof(si);
      si.dwFlags |= STARTF_USESTDHANDLES;
#ifdef STARTF_USESHOWWINDOW
      si.dwFlags |= STARTF_USESHOWWINDOW;
      si.wShowWindow = SW_HIDE;
#endif
      si.hStdOutput = writePipe;
      si.hStdError = writePipe;
      PROCESS_INFORMATION pi{};
      std::string cmdline = "cmd /c " + cmd;
      DWORD creationFlags = 0;
#ifdef CREATE_NO_WINDOW
      creationFlags |= CREATE_NO_WINDOW;
#endif
      if (CreateProcessA(NULL, cmdline.data(), NULL, NULL, TRUE, creationFlags,
                         NULL, NULL, &si, &pi)) {
        CloseHandle(writePipe);
        char buffer[256];
        DWORD readBytes = 0;
        std::string out;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &readBytes, NULL) &&
               readBytes) {
          out.append(buffer, readBytes);
        }
        CloseHandle(readPipe);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (!out.empty()) {
          wxLogMessage("Using pdftotext to extract text from '%s'", path.c_str());
          return out;
        }
        wxLogDebug("pdftotext returned empty output for '%s'; falling back to PoDoFo",
                   path.c_str());
      } else {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        wxLogDebug("pdftotext execution failed for '%s'; falling back to PoDoFo",
                   path.c_str());
      }
    }
#else
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
      char buffer[256];
      std::string out;
      while (fgets(buffer, sizeof(buffer), pipe))
        out += buffer;
      pclose(pipe);
      if (!out.empty()) {
        wxLogMessage("Using pdftotext to extract text from '%s'", path.c_str());
        return out;
      }
      wxLogDebug("pdftotext returned empty output for '%s'; falling back to PoDoFo",
                 path.c_str());
    } else {
      wxLogDebug("pdftotext execution failed for '%s'; falling back to PoDoFo",
                 path.c_str());
    }
#endif
  }

  // Fallback to PoDoFo if pdftotext is unavailable or fails
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
    if (!out.empty()) {
      if (!out.empty() && out.back() == '\n')
        out.pop_back();
      wxLogMessage("Using PoDoFo to extract text from '%s'", path.c_str());
      return out;
    }
  } catch (const PdfError &e) {
    wxLogError("PoDoFo failed to extract text from '%s': %s", path.c_str(),
               e.what());
  }
  return {};
}
