#include "splashscreen.h"
#include "projectutils.h"
#include <algorithm>
#include <filesystem>
#include <wx/artprov.h>
#include <wx/bitmap.h>
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

constexpr int kFallbackIconSize = 256;
constexpr int kRoundedCornerRadius = 16;

wxString PathToWxString(const std::filesystem::path &path) {
  const std::u8string utf8 = path.u8string();
  const std::string utf8Str(utf8.begin(), utf8.end());
  return wxString::FromUTF8(utf8Str.c_str());
}

void LogMissingIcon(const std::filesystem::path &path) {
  wxLogWarning("Splash icon not found at '" + PathToWxString(path) + "'");
}

wxBitmap ScaleBitmapByFactor(const wxBitmap &bitmap, double factor) {
  if (!bitmap.IsOk() || factor <= 0.0 || factor == 1.0)
    return bitmap;

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk())
    return bitmap;

  const int scaledWidth = std::max(1, static_cast<int>(image.GetWidth() * factor));
  const int scaledHeight =
      std::max(1, static_cast<int>(image.GetHeight() * factor));
  image.Rescale(scaledWidth, scaledHeight, wxIMAGE_QUALITY_HIGH);
  return wxBitmap(image);
}

void ApplyRoundedShape(wxFrame *frame, int radius) {
  if (!frame)
    return;

  const wxSize size = frame->GetSize();
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;

  wxBitmap shapeBitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC dc(shapeBitmap);
  dc.SetBackground(*wxBLACK_BRUSH);
  dc.Clear();
  dc.SetBrush(*wxWHITE_BRUSH);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), radius);
  dc.SelectObject(wxNullBitmap);

  frame->SetShape(wxRegion(shapeBitmap, *wxBLACK));
}
} // namespace

void SplashScreen::Show() {
  if (g_splash)
    return;

  g_splash = new wxFrame(nullptr, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                         wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE |
                             wxFRAME_SHAPED);

  wxPanel *panel = new wxPanel(g_splash);
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  wxBitmap logoBmp;
  const std::filesystem::path resourceRoot = ProjectUtils::GetResourceRoot();
  const std::filesystem::path splashLogoPath = resourceRoot / "Perastage_logo.png";
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
    wxIconBundle bundle;
    if (!iconPath.empty() && std::filesystem::exists(iconPath, ec)) {
      bundle.AddIcon(PathToWxString(iconPath), wxBITMAP_TYPE_ICO);
    } else {
      LogMissingIcon(
          iconPath.empty() ? std::filesystem::path("resources/Perastage.ico")
                           : iconPath);
    }
    wxIcon icon = bundle.GetIcon(wxSize(256, 256));
    if (icon.IsOk()) {
      logoBmp = wxBitmap(icon);
    }
  }

  if (!logoBmp.IsOk()) {
    const std::filesystem::path pngPath = resourceRoot / "perastage3d.png";
    if (!resourceRoot.empty() && std::filesystem::exists(pngPath, ec)) {
      logoBmp.LoadFile(PathToWxString(pngPath), wxBITMAP_TYPE_PNG);
    }
    wxIcon icon = bundle.GetIcon(wxSize(kFallbackIconSize, kFallbackIconSize));
    if (icon.IsOk()) {
      logoBmp = wxBitmap(icon);
    }
  }

  if (!logoBmp.IsOk()) {
    logoBmp = wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_OTHER,
                                       wxSize(kFallbackIconSize, kFallbackIconSize));
  }

  wxStaticBitmap *logo = new wxStaticBitmap(panel, wxID_ANY, logoBmp);

  g_label =
      new wxStaticText(panel, wxID_ANY, "Loading Perastage...", wxDefaultPosition,
                       wxDefaultSize, wxALIGN_CENTER);
  wxFont font = g_label->GetFont();
  font.MakeBold();
  g_label->SetFont(font);

  sizer->AddStretchSpacer(1);
  sizer->Add(logo, 0, wxALIGN_CENTER | wxALL, 10);
  sizer->Add(g_label, 0, wxALIGN_CENTER | wxBOTTOM, 20);
  sizer->AddStretchSpacer(1);
  panel->SetSizerAndFit(sizer);

  g_splash->SetClientSize(panel->GetBestSize());
  ApplyRoundedShape(g_splash, kRoundedCornerRadius);
  g_splash->CentreOnScreen();
  g_splash->Show();
  g_splash->Raise();
  g_splash->Update();
}

void SplashScreen::SetMessage(const wxString &msg) {
  if (g_label) {
    g_label->SetLabel(msg);
    g_label->GetParent()->Layout();
    g_label->Refresh();
    g_label->Update();
  }
}

void SplashScreen::Hide() {
  if (g_splash) {
    g_splash->Destroy();
    g_splash = nullptr;
    g_label = nullptr;
  }
}
