#include "splashscreen.h"
#include <wx/artprov.h>
#include <wx/filefn.h>
#include <wx/iconbndl.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/wx.h>

namespace {
wxFrame *g_splash = nullptr;
wxStaticText *g_label = nullptr;
}

void SplashScreen::Show() {
  if (g_splash)
    return;

  g_splash = new wxFrame(nullptr, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                         wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE);

  wxPanel *panel = new wxPanel(g_splash);
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  wxBitmap logoBmp;
  wxIconBundle bundle;
  const wxString iconPaths[] = {"resources/Perastage.ico",
                                "../resources/Perastage.ico",
                                "../../resources/Perastage.ico"};
  for (const wxString &path : iconPaths) {
    if (wxFileExists(path))
      bundle.AddIcon(path, wxBITMAP_TYPE_ICO);
  }
  wxIcon icon = bundle.GetIcon(wxSize(256, 256));
  if (icon.IsOk())
    logoBmp = wxBitmap(icon);
  else
    logoBmp =
        wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_OTHER, wxSize(256, 256));

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
