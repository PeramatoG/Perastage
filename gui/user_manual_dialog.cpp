#include "user_manual_dialog.h"
#include "markdown.h"
#include "mainwindow_menu_text_utils.h"
#include <utility>
#include <algorithm>
#include <wx/button.h>
#include <wx/html/htmlwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

// wxHtmlWindow exposes anchor scrolling only to subclasses.
class ManualHtmlWindow final : public wxHtmlWindow {
public:
  using wxHtmlWindow::wxHtmlWindow;
  using wxHtmlWindow::ScrollToAnchor;
};

UserManualDialog::UserManualDialog(wxWindow *parent,
                                   const std::filesystem::path &manualRoot,
                                   const std::string &uiLanguage,
                                   OpenBrowser browser)
    : wxDialog(parent, wxID_ANY, _("Perastage Help"), wxDefaultPosition,
               wxSize(900, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
      resolver_(manualRoot, uiLanguage), browser_(std::move(browser)) {
  if (parent) {
    const auto size = parent->GetSize();
    SetSize(wxSize(std::max(900, static_cast<int>(size.x * 0.85)),
                   std::max(700, static_cast<int>(size.y * 0.85))));
  }
  if (!browser_) browser_ = [](const std::string &url) {
    wxLaunchDefaultBrowser(wxString::FromUTF8(url));
  };
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  auto *navigation = new wxBoxSizer(wxHORIZONTAL);
  back_ = new wxButton(this, wxID_ANY, _("Back"));
  auto *home = new wxButton(this, wxID_ANY, _("Home"));
  auto *online = new wxButton(this, wxID_ANY, _("Online Documentation"));
  navigation->Add(back_, 0, wxRIGHT, 6);
  navigation->Add(home, 0, wxRIGHT, 6);
  navigation->AddStretchSpacer();
  navigation->Add(online, 0);
  sizer->Add(navigation, 0, wxEXPAND | wxALL, 8);
  error_ = new wxStaticText(this, wxID_ANY,
      _("Offline Help files are missing. Use Online Documentation."));
  sizer->Add(error_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  html_ = new ManualHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                          wxHW_SCROLLBAR_AUTO);
  sizer->Add(html_, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
  back_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { GoBack(); });
  home->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { GoHome(); });
  online->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
    browser_(user_manual::kOnlineDocumentation);
  });
  html_->Bind(wxEVT_HTML_LINK_CLICKED, [this](wxHtmlLinkEvent &event) {
    NavigateLink(WxToUtf8(event.GetLinkInfo().GetHref()));
  });
  back_->Enable(false);
  GoHome();
}

bool UserManualDialog::OpenPage(const std::string &page,
                                const std::string &fragment, bool remember) {
  const auto loaded = resolver_.Load(page, fragment);
  error_->Show(!loaded);
  Layout();
  if (!loaded) return false;
  if (remember && !current_.name.empty()) history_.push_back(current_);
  current_ = *loaded;
  html_->SetPage(wxString::FromUTF8(WrapHelpHtml(MarkdownToHtml(current_.markdown))));
  if (!fragment.empty()) html_->ScrollToAnchor(wxString::FromUTF8(fragment));
  back_->Enable(!history_.empty());
  return true;
}

bool UserManualDialog::NavigateLink(const std::string &href) {
  const auto link = resolver_.ResolveLink(current_.name, href);
  switch (link.kind) {
  case user_manual::LinkKind::Manual:
    return OpenPage(link.target, link.fragment, true);
  case user_manual::LinkKind::Fragment:
    if (html_->ScrollToAnchor(wxString::FromUTF8(link.fragment))) {
      current_.fragment = link.fragment;
      return true;
    }
    return false;
  case user_manual::LinkKind::Online:
    browser_(link.target);
    return true;
  case user_manual::LinkKind::Rejected:
    return false;
  }
  return false;
}

bool UserManualDialog::GoHome() { return OpenPage("index.md", {}, !current_.name.empty()); }

bool UserManualDialog::GoBack() {
  if (history_.empty()) return false;
  const auto previous = history_.back();
  if (!OpenPage(previous.name, previous.fragment, false)) return false;
  history_.pop_back();
  back_->Enable(!history_.empty());
  return true;
}
