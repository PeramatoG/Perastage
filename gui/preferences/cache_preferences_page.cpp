#include "preferences/cache_preferences_page.h"
#include "apppaths.h"
#include "cache_service.h"
#include "filesystem_path_utils.h"
#include <wx/button.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

CachePreferencesPage::CachePreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  auto *layout = new wxBoxSizer(wxVERTICAL);
  layout->Add(new wxStaticText(this, wxID_ANY, _("Cache location:")),
              0, wxALL, FromDIP(10));
  auto *location = new wxTextCtrl(this, wxID_ANY,
      wxString::FromUTF8(PathUtils::PathToUtf8(AppPaths::GetCacheRoot())),
      wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  location->SetMinSize(wxSize(FromDIP(100), -1));
  layout->Add(location, 0, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(10));
  sizeLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
  layout->Add(sizeLabel, 0, wxALL, FromDIP(10));
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  auto *clear = new wxButton(this, wxID_ANY, _("Clear Cache..."));
  auto *open = new wxButton(this, wxID_ANY, _("Open Cache Folder"));
  buttons->Add(clear, 0, wxRIGHT, FromDIP(10));
  buttons->Add(open);
  layout->Add(buttons, 0, wxALL, FromDIP(10));
  layout->Add(new wxStaticText(this, wxID_ANY,
      _("Cache maintenance takes effect immediately. Cached geometry and the GDTF catalog will be regenerated when needed.")),
      0, wxALL | wxEXPAND, FromDIP(10));
  SetSizer(layout);
  clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ClearCache(); });
  open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OpenFolder(); });
  PrepareLayout();
}

void CachePreferencesPage::LoadPreferences(const IGuiPreferencesService &) {
  RefreshSize();
}
void CachePreferencesPage::ApplyPreferences(IGuiPreferencesService &) {}

void CachePreferencesPage::RefreshSize() {
  const auto statistics = CacheService::GetStatistics();
  const auto size = wxFileName::GetHumanReadableSize(wxULongLong(statistics.bytes));
  sizeLabel->SetLabel(wxString::Format(
      statistics.Succeeded() ? _("Total cache size: %s")
                             : _("Total cache size: %s (some files could not be read)"), size));
  Layout();
}

void CachePreferencesPage::ClearCache() {
  if (wxMessageBox(
          _("Clear all Perastage caches? Geometry will be regenerated and the GDTF catalog will be downloaded again when online. Your settings, libraries and projects will be preserved."),
          _("Clear Cache"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
    return;
  const auto result = CacheService::Clear();
  RefreshSize();
  if (!result.Succeeded()) {
    wxString details;
    for (const auto &error : result.errors)
      details += wxString::FromUTF8(error) + "\n";
    wxMessageBox(wxString::Format(
        _("Some cache contents could not be cleared:\n%s"), details),
        _("Clear Cache"), wxOK | wxICON_WARNING, this);
  }
}

void CachePreferencesPage::OpenFolder() {
  if (!CacheService::EnsureDirectories() ||
      !wxLaunchDefaultApplication(wxString::FromUTF8(
          PathUtils::PathToUtf8(AppPaths::GetCacheRoot()))))
    wxMessageBox(_("The cache folder could not be opened."), _("Cache"),
                 wxOK | wxICON_WARNING, this);
}
