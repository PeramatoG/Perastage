#include "splashscreen.h"
#include "app_version.h"
#include "logger.h"
#include "projectutils.h"
#include <algorithm>
#include <filesystem>
#include <wx/artprov.h>
#include <wx/dcmemory.h>
#include <wx/iconbndl.h>
#include <wx/log.h>
#include <wx/region.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/wx.h>

namespace {
wxFrame *g_splash = nullptr;
wxStaticText *g_label = nullptr;
constexpr int kSplashCornerRadius = 16;
constexpr int kSplashLogoMaxSize = 180;

// Converts a filesystem path into a UTF-8 wxString suitable for wxWidgets APIs.
wxString PathToWxString(const std::filesystem::path &path) {
  const std::u8string utf8 = path.u8string();
  const std::string utf8Str(utf8.begin(), utf8.end());
  return wxString::FromUTF8(utf8Str.c_str());
}

// Logs a warning message when an icon resource cannot be found on disk.
void LogMissingIcon(const std::filesystem::path &path) {
  const wxString message =
      "Splash icon not found at '" + PathToWxString(path) + "'";
  Logger::Instance().Log(Logger::Level::Warn, message.ToStdString());
}

// Scales a bitmap down to fit within a maximum size while preserving aspect ratio.
wxBitmap ScaleDownBitmap(const wxBitmap &bitmap, int maxSize) {
  if (!bitmap.IsOk() || maxSize <= 0)
    return bitmap;

  const wxSize size = bitmap.GetSize();
  const int width = size.GetWidth();
  const int height = size.GetHeight();
  if (width <= 0 || height <= 0)
    return bitmap;

  const int largestSide = std::max(width, height);
  if (largestSide <= maxSize)
    return bitmap;

  const double scale = static_cast<double>(maxSize) /
                       static_cast<double>(largestSide);
  const int targetWidth = std::max(1, static_cast<int>(width * scale));
  const int targetHeight = std::max(1, static_cast<int>(height * scale));
  const wxImage scaled = bitmap.ConvertToImage().Scale(
      targetWidth, targetHeight, wxIMAGE_QUALITY_HIGH);
  return wxBitmap(scaled);
}

// Applies a rounded-corner window region to the splash frame.
void ApplyRoundedShape(wxFrame *frame, int radius) {
  if (!frame)
    return;

  const wxSize size = frame->GetClientSize();
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;

  wxBitmap maskBitmap(size.GetWidth(), size.GetHeight());
  wxMemoryDC dc(maskBitmap);
  dc.SetBackground(*wxBLACK_BRUSH);
  dc.Clear();
  dc.SetBrush(*wxWHITE_BRUSH);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), radius);
  dc.SelectObject(wxNullBitmap);

  const wxRegion region(maskBitmap, *wxBLACK);
  frame->SetShape(region);
}

// Builds the splash logo bitmap from resources with safe fallbacks for missing assets.
wxBitmap BuildSplashBitmap() {
  wxBitmap logoBmp;
  const std::filesystem::path resourceRoot = ProjectUtils::GetResourceRoot();
  const std::filesystem::path splashLogoPath =
      resourceRoot / "Perastage_logo.png";
  std::error_code ec;
  if (!resourceRoot.empty() && std::filesystem::exists(splashLogoPath, ec)) {
    logoBmp.LoadFile(PathToWxString(splashLogoPath), wxBITMAP_TYPE_PNG);
  }

  if (!logoBmp.IsOk()) {
    LogMissingIcon(splashLogoPath.empty()
                       ? std::filesystem::path("resources/Perastage_logo.png")
                       : splashLogoPath);

    std::filesystem::path iconPath;
    if (!resourceRoot.empty())
      iconPath = resourceRoot / "Perastage.ico";
    if (!iconPath.empty() && std::filesystem::exists(iconPath, ec)) {
      wxIcon icon;
      icon.LoadFile(PathToWxString(iconPath), wxBITMAP_TYPE_ICO);
      if (icon.IsOk()) {
        logoBmp = wxBitmap(icon);
      }
    } else {
      LogMissingIcon(
          iconPath.empty() ? std::filesystem::path("resources/Perastage.ico")
                           : iconPath);
    }
  }

  if (!logoBmp.IsOk()) {
    logoBmp = wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_OTHER,
                                       wxSize(256, 256));
  }

  return ScaleDownBitmap(logoBmp, kSplashLogoMaxSize);
}
}

// Creates and displays the splash window with app branding and loading status text.
void SplashScreen::Show() {
  if (g_splash)
    return;

  g_splash = new wxFrame(nullptr, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                         wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE |
                             wxFRAME_SHAPED);

  wxPanel *panel = new wxPanel(g_splash);
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  wxBitmap logoBmp = BuildSplashBitmap();
  wxStaticBitmap *logo = new wxStaticBitmap(panel, wxID_ANY, logoBmp);
  wxStaticText *appInfoLabel =
      new wxStaticText(panel, wxID_ANY,
                       wxString::Format("%s %s", app::kName, app::kVersionDisplay),
                       wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
  wxFont appInfoFont = appInfoLabel->GetFont();
  appInfoFont.MakeBold();
  appInfoLabel->SetFont(appInfoFont);

  g_label =
      new wxStaticText(panel, wxID_ANY, "Loading Perastage...", wxDefaultPosition,
                       wxDefaultSize, wxALIGN_CENTER);
  wxFont font = g_label->GetFont();
  font.MakeBold();
  g_label->SetFont(font);

  sizer->AddStretchSpacer(1);
  sizer->Add(appInfoLabel, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 10);
  sizer->Add(logo, 0, wxALIGN_CENTER | wxALL, 10);
  sizer->Add(g_label, 0, wxALIGN_CENTER | wxBOTTOM, 20);
  sizer->AddStretchSpacer(1);
  panel->SetSizerAndFit(sizer);

  g_splash->SetClientSize(panel->GetBestSize());
  ApplyRoundedShape(g_splash, kSplashCornerRadius);
  g_splash->CentreOnScreen();
  g_splash->Show();
  g_splash->Raise();
  g_splash->Update();
}

// Updates the splash loading message and refreshes the label in place.
void SplashScreen::SetMessage(const wxString &msg) {
  if (g_label) {
    g_label->SetLabel(msg);
    g_label->GetParent()->Layout();
    g_label->Refresh();
    g_label->Update();
  }
}

// Closes and destroys the splash window, clearing retained global pointers.
void SplashScreen::Hide() {
  if (g_splash) {
    g_splash->Destroy();
    g_splash = nullptr;
    g_label = nullptr;
  }
}
