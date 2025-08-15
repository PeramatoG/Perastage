#include "splashscreen.h"
#include <wx/dcmemory.h>
#include <wx/wx.h>

namespace {
wxSplashScreen *g_splash = nullptr;
}

void SplashScreen::Show() {
  if (g_splash)
    return;
  wxBitmap bmp(400, 300);
  {
    wxMemoryDC dc(bmp);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    dc.SetTextForeground(*wxBLACK);
    dc.SetFont(wxFontInfo(14).Bold());
    dc.DrawLabel("Loading Perastage...", wxRect(0, 0, 400, 300),
                 wxALIGN_CENTER);
  }
  g_splash =
      new wxSplashScreen(bmp, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT,
                         0, nullptr, wxID_ANY);
}

void SplashScreen::Hide() {
  if (g_splash) {
    g_splash->Destroy();
    g_splash = nullptr;
  }
}
